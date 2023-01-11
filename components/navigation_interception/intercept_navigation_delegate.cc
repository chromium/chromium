// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_delegate.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/escape.h"
#include "components/navigation_interception/jni_headers/InterceptNavigationDelegate_jni.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;
using ui::PageTransition;

namespace navigation_interception {

namespace {

const void* const kInterceptNavigationDelegateUserDataKey =
    &kInterceptNavigationDelegateUserDataKey;

bool CheckIfShouldIgnoreNavigationOnUIThread(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(navigation_handle);

  InterceptNavigationDelegate* intercept_navigation_delegate =
      InterceptNavigationDelegate::Get(navigation_handle->GetWebContents());
  if (!intercept_navigation_delegate)
    return false;

  return intercept_navigation_delegate->ShouldIgnoreNavigation(
      navigation_handle);
}

class RedirectURLLoader : public network::mojom::URLLoader {
 public:
  RedirectURLLoader(const GURL& url,
                    const network::ResourceRequest& resource_request,
                    mojo::PendingRemote<network::mojom::URLLoaderClient> client)
      : client_(std::move(client)) {
    net::HttpStatusCode response_code = net::HTTP_TEMPORARY_REDIRECT;
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->encoded_data_length = 0;
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders("HTTP/1.1 307 Temporary Redirect"));

    // Avoid a round-trip to the network service by pre-parsing headers.
    // This doesn't violate: `docs/security/rule-of-2.md`, because the input is
    // trusted, before appending the Location: <url> header.
    response_head->parsed_headers =
        network::PopulateParsedHeaders(response_head->headers.get(), url);

    response_head->headers->AddHeader("Location", url.spec());

    auto first_party_url_policy =
        resource_request.update_first_party_url_on_redirect
            ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
            : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL;

    client_->OnReceiveRedirect(
        net::RedirectInfo::ComputeRedirectInfo(
            resource_request.method, resource_request.url,
            resource_request.site_for_cookies, first_party_url_policy,
            resource_request.referrer_policy, resource_request.referrer.spec(),
            response_code, url, absl::nullopt,
            /*insecure_scheme_was_upgraded=*/false,
            /*copy_fragment=*/false),
        std::move(response_head));
  }

  RedirectURLLoader(const RedirectURLLoader&) = delete;
  RedirectURLLoader& operator=(const RedirectURLLoader&) = delete;

  ~RedirectURLLoader() override = default;

 private:
  // network::mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override {
    NOTREACHED();
  }
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  mojo::Remote<network::mojom::URLLoaderClient> client_;
};

void RedirectToCallback(
    GURL url,
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> pending_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> pending_client) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<RedirectURLLoader>(url, resource_request,
                                          std::move(pending_client)),
      std::move(pending_receiver));
}

}  // namespace

// static
void InterceptNavigationDelegate::Associate(
    WebContents* web_contents,
    std::unique_ptr<InterceptNavigationDelegate> delegate) {
  web_contents->SetUserData(kInterceptNavigationDelegateUserDataKey,
                            std::move(delegate));
}

// static
InterceptNavigationDelegate* InterceptNavigationDelegate::Get(
    WebContents* web_contents) {
  return static_cast<InterceptNavigationDelegate*>(
      web_contents->GetUserData(kInterceptNavigationDelegateUserDataKey));
}

// static
std::unique_ptr<content::NavigationThrottle>
InterceptNavigationDelegate::MaybeCreateThrottleFor(
    content::NavigationHandle* handle,
    navigation_interception::SynchronyMode mode) {
  // Navigations in a subframe or non-primary frame tree should not be
  // intercepted. As examples of a non-primary frame tree, a navigation
  // occurring in a Portal element or an unactivated prerendering page should
  // not launch an app.
  // TODO(bokan): This is a bit of a stopgap approach since we won't run
  // throttles again when the prerender is activated which means links that are
  // prerendered will avoid launching an app intent that a regular navigation
  // would have. Longer term we'll want prerender activation to check for app
  // intents, or have this throttle cancel the prerender if an intent would
  // have been launched (without launching the intent). It's also not clear
  // what the right behavior for <portal> elements is.
  // https://crbug.com/1227659.
  if (!handle->IsInPrimaryMainFrame())
    return nullptr;

  return std::make_unique<InterceptNavigationThrottle>(
      handle, base::BindRepeating(&CheckIfShouldIgnoreNavigationOnUIThread),
      mode);
}

InterceptNavigationDelegate::InterceptNavigationDelegate(
    JNIEnv* env,
    jobject jdelegate,
    bool escape_external_handler_value)
    : weak_jdelegate_(env, jdelegate),
      escape_external_handler_value_(escape_external_handler_value) {}

InterceptNavigationDelegate::~InterceptNavigationDelegate() = default;

bool InterceptNavigationDelegate::ShouldIgnoreNavigation(
    content::NavigationHandle* navigation_handle) {
  GURL escaped_url = escape_external_handler_value_
                         ? GURL(base::EscapeExternalHandlerValue(
                               navigation_handle->GetURL().spec()))
                         : navigation_handle->GetURL();

  if (!escaped_url.is_valid())
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = weak_jdelegate_.get(env);

  if (jdelegate.is_null())
    return false;

  return Java_InterceptNavigationDelegate_shouldIgnoreNavigation(
      env, jdelegate, navigation_handle->GetJavaNavigationHandle(),
      url::GURLAndroid::FromNativeGURL(env, escaped_url));
}

void InterceptNavigationDelegate::HandleSubframeExternalProtocol(
    const GURL& url,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const absl::optional<url::Origin>& initiating_origin,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  GURL escaped_url = escape_external_handler_value_
                         ? GURL(base::EscapeExternalHandlerValue(url.spec()))
                         : url;
  if (!escaped_url.is_valid())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = weak_jdelegate_.get(env);

  if (jdelegate.is_null())
    return;
  ScopedJavaLocalRef<jobject> j_gurl =
      Java_InterceptNavigationDelegate_handleSubframeExternalProtocol(
          env, jdelegate, url::GURLAndroid::FromNativeGURL(env, escaped_url),
          page_transition, has_user_gesture,
          initiating_origin ? initiating_origin->CreateJavaObject() : nullptr);
  if (j_gurl.is_null())
    return;
  std::unique_ptr<GURL> gurl = url::GURLAndroid::ToNativeGURL(env, j_gurl);

  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
      out_factory->InitWithNewPipeAndPassReceiver();
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
          base::BindOnce(&RedirectToCallback, *gurl));
  loader_factory->Clone(std::move(receiver));
}

void InterceptNavigationDelegate::OnResourceRequestWithGesture() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = weak_jdelegate_.get(env);
  if (jdelegate.is_null())
    return;
  Java_InterceptNavigationDelegate_onResourceRequestWithGesture(env, jdelegate);
}

}  // namespace navigation_interception
