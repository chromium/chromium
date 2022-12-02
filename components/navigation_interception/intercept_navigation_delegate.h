// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_
#define COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/supports_user_data.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "ui/base/page_transition_types.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class WebContents;
}

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
                              jobject jdelegate,
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
      const absl::optional<url::Origin>& initiating_origin,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory);

  // To be called when a main frame requests a resource with a user gesture (eg.
  // xrh, fetch, etc.)
  void OnResourceRequestWithGesture();

 private:
  JavaObjectWeakGlobalRef weak_jdelegate_;
  bool escape_external_handler_value_ = false;
};

}  // namespace navigation_interception

#endif  // COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_DELEGATE_H_
