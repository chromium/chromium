// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_
#define COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "ui/base/page_transition_types.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class WebContents;
}

namespace network {
struct ResourceRequest;

namespace mojom {
class URLLoader;
class URLLoaderClient;
}  // namespace mojom
}  // namespace network

namespace url {
class Origin;
}

class GURL;

namespace navigation_interception {

// Native side of the InterceptNavigationDelegate Java interface.
// This is used to create a InterceptNavigationResourceThrottle that calls the
// Java interface method to determine whether a navigation should be ignored or
// not.
// To us this class:
// 1) the Java-side interface implementation must be associated (via the
//    Associate method) with a WebContents for which URLRequests are to be
//    intercepted,
// 2) the NavigationThrottle obtained via MaybeCreateThrottleFor must be
//    associated with the NavigationHandle in the ContentBrowserClient
//    implementation.
class InterceptNavigationDelegate : public base::SupportsUserData::Data {
 public:
  // Pass true for |escape_external_handler_value| to have
  // base::EscapeExternalHandlerValue() invoked on URLs passed to
  // ShouldIgnoreNavigation() before the navigation is processed.
  InterceptNavigationDelegate(JNIEnv* env,
                              const jni_zero::JavaRef<jobject>& jdelegate,
                              bool escape_external_handler_value = false);

  InterceptNavigationDelegate(const InterceptNavigationDelegate&) = delete;
  InterceptNavigationDelegate& operator=(const InterceptNavigationDelegate&) =
      delete;

  ~InterceptNavigationDelegate() override;

  // Associates the InterceptNavigationDelegate with a WebContents using the
  // SupportsUserData mechanism.
  // As implied by the use of scoped_ptr, the WebContents will assume ownership
  // of |delegate|.
  static void Associate(content::WebContents* web_contents,
                        std::unique_ptr<InterceptNavigationDelegate> delegate);
  // Gets the InterceptNavigationDelegate associated with the WebContents,
  // can be null.
  static InterceptNavigationDelegate* Get(content::WebContents* web_contents);

  // Creates a InterceptNavigationThrottle that will direct all callbacks to
  // the InterceptNavigationDelegate.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle,
      navigation_interception::SynchronyMode mode);

  bool ShouldIgnoreNavigation(content::NavigationHandle* navigation_handle);

  // See ContentBrowserClient::HandleExternalProtocol for the semantics around
  // |out_factory|.
  virtual void HandleSubframeExternalProtocol(
      const GURL& url,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory);

  // To be called when a main frame requests a resource with a user gesture (eg.
  // xrh, fetch, etc.)
  void OnResourceRequestWithGesture();

  void OnSubframeAsyncActionTaken(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_gurl);

 private:
  void LoaderCallback(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> pending_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> pending_client);

  void MaybeHandleSubframeAction();

  JavaObjectWeakGlobalRef weak_jdelegate_;
  bool escape_external_handler_value_ = false;

  mojo::SelfOwnedReceiverRef<network::mojom::URLLoader> url_loader_;
  // An empty URL if an async action is pending, or a URL to redirect to when
  // the URLLoader is ready.
  std::unique_ptr<GURL> subframe_redirect_url_;
  base::WeakPtrFactory<InterceptNavigationDelegate> weak_ptr_factory_{this};
};

}  // namespace navigation_interception

#endif  // COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_
