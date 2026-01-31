// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CUSTOM_HANDLERS_PROTOCOL_HANDLER_NAVIGATION_THROTTLE_H_
#define COMPONENTS_CUSTOM_HANDLERS_PROTOCOL_HANDLER_NAVIGATION_THROTTLE_H_

#include "components/custom_handlers/protocol_handler_registry.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace custom_handlers {

// The navigation throttle for managing custom handlers during navigation, used
// to ensure the user granted permission to the default protocol handler
// registered for the navigation request's url scheme. The ongoing navigation
// request would be deferred until the user makes a decision, proceeding if it
// grants permission and cancelling it otherwise.
class ProtocolHandlerNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit ProtocolHandlerNavigationThrottle(
      content::NavigationThrottleRegistry&,
      ProtocolHandlerRegistry&);
  ~ProtocolHandlerNavigationThrottle() override;

  ProtocolHandlerNavigationThrottle(const ProtocolHandlerNavigationThrottle&) =
      delete;
  ProtocolHandlerNavigationThrottle& operator=(
      const ProtocolHandlerNavigationThrottle&) = delete;

  // content::NavigationThrottle implementation:
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

  // If there is an "unconfirmed" handler for the Navigation Request's url, a
  // new NavigationThrottle will be created and added to interrupt the
  // navigation process to prompt the user about the registered custom handler.
  static void MaybeCreateAndAdd(ProtocolHandlerRegistry*,
                                content::NavigationThrottleRegistry&);

  typedef base::OnceCallback<void(bool save)> HandlerPermissionGrantedCallback;
  typedef base::OnceCallback<void()> HandlerPermissionDeniedCallback;

  // Only for testing.
  using LaunchCallbackForTesting = base::RepeatingCallback<void(
      HandlerPermissionGrantedCallback granted_callback,
      HandlerPermissionDeniedCallback denied_callback)>;
  static LaunchCallbackForTesting& GetDialogLaunchCallbackForTesting();

  virtual void RunConfirmProtocolHandlerDialog(
      content::WebContents* web_contents,
      const ProtocolHandler& handler,
      const std::optional<url::Origin>& initiating_origin,
      HandlerPermissionGrantedCallback granted_callback,
      HandlerPermissionDeniedCallback denied_callback) const {}

 private:
  content::NavigationThrottle::ThrottleCheckResult
  RequestPermissionForHandler();
  void OnProtocolHandlerPermissionGranted(const GURL& target_url, bool save);
  void OnProtocolHandlerPermissionDenied();

  // The ProtocolHandlerRegistry instance is a KeyedService which ownership is
  // managed by the BrowserContext. BrowserContext can be destroyed before this
  // throttle.
  base::WeakPtr<ProtocolHandlerRegistry> protocol_handler_registry_;

  base::WeakPtrFactory<ProtocolHandlerNavigationThrottle> weak_factory_{this};
};

}  // namespace custom_handlers

#endif  // COMPONENTS_CUSTOM_HANDLERS_PROTOCOL_HANDLER_NAVIGATION_THROTTLE_H_
