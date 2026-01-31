// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/protocol_handler_navigation_throttle.h"

#include "base/check_is_test.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace custom_handlers {

ProtocolHandlerNavigationThrottle::ProtocolHandlerNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    ProtocolHandlerRegistry& protocol_handler_registry)
    : content::NavigationThrottle(registry),
      protocol_handler_registry_(protocol_handler_registry.GetWeakPtr()) {}

ProtocolHandlerNavigationThrottle::~ProtocolHandlerNavigationThrottle() =
    default;

const char* ProtocolHandlerNavigationThrottle::GetNameForLogging() {
  return "ProtocolHandlerNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
ProtocolHandlerNavigationThrottle::WillStartRequest() {
  if (!protocol_handler_registry_) {
    return CANCEL;
  }
  return RequestPermissionForHandler();
}

content::NavigationThrottle::ThrottleCheckResult
ProtocolHandlerNavigationThrottle::WillRedirectRequest() {
  if (!protocol_handler_registry_) {
    return CANCEL;
  }
  return RequestPermissionForHandler();
}

// static
void ProtocolHandlerNavigationThrottle::MaybeCreateAndAdd(
    ProtocolHandlerRegistry* protocol_handler_registry,
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  const GURL& url = handle.GetURL();
  // TODO(crbug.com/40482153): We should use scheme_piece instead, which would
  // imply adapting the ProtocolHandlerRegistry code to use std::string_view.
  if (!protocol_handler_registry ||
      !protocol_handler_registry->IsHandledProtocol(url.scheme()) ||
      protocol_handler_registry->IsProtocolHandlerConfirmed(url.scheme())) {
    return;
  }
  registry.AddThrottle(std::make_unique<ProtocolHandlerNavigationThrottle>(
      registry, *protocol_handler_registry));
}

// static
ProtocolHandlerNavigationThrottle::LaunchCallbackForTesting&
ProtocolHandlerNavigationThrottle::GetDialogLaunchCallbackForTesting() {
  static base::NoDestructor<LaunchCallbackForTesting> callback;
  return *callback;
}

content::NavigationThrottle::ThrottleCheckResult
ProtocolHandlerNavigationThrottle::RequestPermissionForHandler() {
  const GURL& url = navigation_handle()->GetURL();
  if (!protocol_handler_registry_->IsHandledProtocol(url.scheme()) ||
      protocol_handler_registry_->IsProtocolHandlerConfirmed(url.scheme())) {
    return PROCEED;
  }

  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  ProtocolHandler handler =
      protocol_handler_registry_->GetHandlerFor(url.scheme());

  HandlerPermissionGrantedCallback permission_granted_callback = base::BindOnce(
      &ProtocolHandlerNavigationThrottle::OnProtocolHandlerPermissionGranted,
      weak_factory_.GetWeakPtr(), url);
  HandlerPermissionDeniedCallback permission_denied_callback = base::BindOnce(
      &ProtocolHandlerNavigationThrottle::OnProtocolHandlerPermissionDenied,
      weak_factory_.GetWeakPtr());

  base::OnceClosure launch_callback = base::BindOnce(
      [](base::WeakPtr<ProtocolHandlerNavigationThrottle> throttle,
         content::WebContents* web_contents, const ProtocolHandler& handler,
         const GURL& url, HandlerPermissionGrantedCallback granted_callback,
         HandlerPermissionDeniedCallback denied_callback) {
        if (!throttle) {
          return;
        }
        auto& callback_for_test =
            GetDialogLaunchCallbackForTesting();              // IN-TEST
        if (callback_for_test) {                              // IN-TEST
          CHECK_IS_TEST();                                    // IN-TEST
          callback_for_test.Run(std::move(granted_callback),  // IN-TEST
                                std::move(denied_callback));  // IN-TEST
        } else {
          CHECK_IS_NOT_TEST();
          throttle->RunConfirmProtocolHandlerDialog(
              web_contents, handler, url::Origin::Create(url),
              std::move(granted_callback), std::move(denied_callback));
        }
      },
      weak_factory_.GetWeakPtr(), web_contents, handler, url,
      std::move(permission_granted_callback),
      std::move(permission_denied_callback));

  // We want a modal prompt dialog to be launched here, hence a SingleThread
  // runner is more convenient.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(launch_callback)));

  return DEFER;
}

void ProtocolHandlerNavigationThrottle::OnProtocolHandlerPermissionGranted(
    const GURL& target_url,
    bool remember) {
  protocol_handler_registry_->ConfirmProtocolHandler(target_url.scheme(),
                                                     remember);
  Resume();
}

void ProtocolHandlerNavigationThrottle::OnProtocolHandlerPermissionDenied() {
  CancelDeferredNavigation(CANCEL);
}

}  // namespace custom_handlers
