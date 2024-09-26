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
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_visibility_state.h"
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

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/navigation_interception/jni_headers/InterceptNavigationDelegate_jni.h"

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
  RedirectURLLoader(const network::ResourceRequest& resource_request,
                    mojo::PendingRemote<network::mojom::URLLoaderClient> client)
      : client_(std::move(client)), request_(resource_request) {}

  void DoRedirect(std::unique_ptr<GURL> url) {
    net::HttpStatusCode response_code = net::HTTP_TEMPORARY_REDIRECT;
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->encoded_data_length = 0;
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders("HTTP/1.1 307 Temporary Redirect"));

    // Avoid a round-trip to the network service by pre-parsing headers.
    // This doesn't violate: `docs/security/rule-of-2.md`, because the input is
    // trusted, before appending the Location: <url> header.
    response_head->parsed_headers =
        network::PopulateParsedHeaders(response_head->headers.get(), *url);

    response_head->headers->AddHeader("Location", url->spec());

    auto first_party_url_policy =
        request_.update_first_party_url_on_redirect
            ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
            : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL;

    client_->OnReceiveRedirect(
        net::RedirectInfo::ComputeRedirectInfo(
            request_.method, request_.url, request_.site_for_cookies,
            first_party_url_policy, request_.referrer_policy,
            request_.referrer.spec(), response_code, *url, std::nullopt,
            /*insecure_scheme_was_upgraded=*/false,
            /*copy_fragment=*/false),
        std::move(response_head));
  }

  void OnNonRedirectAsyncAction() {
    client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
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
      const std::optional<GURL>& new_url) override {
    NOTREACHED_IN_MIGRATION();
  }
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  mojo::Remote<network::mojom::URLLoaderClient> client_;
  network::ResourceRequest request_;
};

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
    const jni_zero::JavaRef<jobject>& jdelegate,
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

  bool hidden_cross_frame = false;
  // Only main frame navigations use this path, so we only need to check if the
  // navigation is cross-frame to the main frame.
  if (navigation_handle->GetInitiatorFrameToken() &&
      navigation_handle->GetInitiatorFrameToken() !=
          navigation_handle->GetWebContents()
              ->GetPrimaryMainFrame()
              ->GetFrameToken()) {
    content::RenderFrameHost* initiator_frame_host =
        content::RenderFrameHost::FromFrameToken(
            content::GlobalRenderFrameHostToken(
                navigation_handle->GetInitiatorProcessId(),
                navigation_handle->GetInitiatorFrameToken().value()));
    // If the initiator is gone treat it as not visible.
    hidden_cross_frame =
        !initiator_frame_host || initiator_frame_host->GetVisibilityState() !=
                                     content::PageVisibilityState::kVisible;
  }

  // We don't care which sandbox flags are present, only that any sandbox flags
  // are present, as we don't support persisting sandbox flags through fallback
  // URL navigation.
  bool is_sandboxed = navigation_handle->SandboxFlagsInherited() !=
                          network::mojom::WebSandboxFlags::kNone ||
                      navigation_handle->SandboxFlagsInitiator() !=
                          network::mojom::WebSandboxFlags::kNone;

  return Java_InterceptNavigationDelegate_shouldIgnoreNavigation(
      env, jdelegate, navigation_handle->GetJavaNavigationHandle(),
      url::GURLAndroid::FromNativeGURL(env, escaped_url), hidden_cross_frame,
      is_sandboxed);
}

void InterceptNavigationDelegate::HandleSubframeExternalProtocol(
    const GURL& url,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const std::optional<url::Origin>& initiating_origin,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  // If there's a pending async subframe action, don't consider external
  // navigation for the current navigation.
  if (subframe_redirect_url_ || url_loader_) {
    return;
  }

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
          initiating_origin ? initiating_origin->ToJavaObject(env) : nullptr);
  if (j_gurl.is_null())
    return;
  subframe_redirect_url_ =
      std::make_unique<GURL>(url::GURLAndroid::ToNativeGURL(env, j_gurl));

  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
      out_factory->InitWithNewPipeAndPassReceiver();
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
          base::BindOnce(&InterceptNavigationDelegate::LoaderCallback,
                         weak_ptr_factory_.GetWeakPtr()));
  loader_factory->Clone(std::move(receiver));
}

void InterceptNavigationDelegate::LoaderCallback(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> pending_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> pending_client) {
  url_loader_ = mojo::MakeSelfOwnedReceiver(
      std::make_unique<RedirectURLLoader>(resource_request,
                                          std::move(pending_client)),
      std::move(pending_receiver));
  MaybeHandleSubframeAction();
}

void InterceptNavigationDelegate::MaybeHandleSubframeAction() {
  // An empty subframe_redirect_url_ implies a pending async action.
  if (!url_loader_ ||
      (subframe_redirect_url_ && subframe_redirect_url_->is_empty())) {
    return;
  }
  RedirectURLLoader* loader =
      static_cast<RedirectURLLoader*>(url_loader_->impl());
  if (!subframe_redirect_url_) {
    loader->OnNonRedirectAsyncAction();
  } else {
    loader->DoRedirect(std::move(subframe_redirect_url_));
  }
  url_loader_.reset();
}

void InterceptNavigationDelegate::OnResourceRequestWithGesture() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = weak_jdelegate_.get(env);
  if (jdelegate.is_null())
    return;
  Java_InterceptNavigationDelegate_onResourceRequestWithGesture(env, jdelegate);
}

void InterceptNavigationDelegate::OnSubframeAsyncActionTaken(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_gurl) {
  // subframe_redirect_url_ no longer empty indicates the async action has been
  // taken.
  subframe_redirect_url_ =
      j_gurl.is_null()
          ? nullptr
          : std::make_unique<GURL>(url::GURLAndroid::ToNativeGURL(env, j_gurl));
  MaybeHandleSubframeAction();
}

}  // namespace navigation_interception
