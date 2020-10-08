// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
#define COMPONENTS_PERMISSIONS_NOTIFICATION_PERMISSION_UI_SELECTOR_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "components/permissions/permission_request.h"

namespace permissions {

// The interface for implementations that decide if the quiet prompt UI should
// be used to display a notification permission |request|, whether a warning
// should be printed to the Dev Tools console, and the reasons for both.
//
// Implementations of interface are expected to have long-lived instances that
// can support multiple requests, but only one at a time.
class NotificationPermissionUiSelector {
 public:
  enum class QuietUiReason {
    kEnabledInPrefs,
    kTriggeredByCrowdDeny,
    kTriggeredDueToAbusiveRequests,
    kTriggeredDueToAbusiveContent,
  };

  enum class WarningReason {
    kAbusiveRequests,
    kAbusiveContent,
  };

  struct Decision {
    Decision(base::Optional<QuietUiReason> quiet_ui_reason,
             base::Optional<WarningReason> warning_reason);
    ~Decision();

    Decision(const Decision&);
    Decision& operator=(const Decision&);

    static constexpr base::Optional<QuietUiReason> UseNormalUi() {
      return base::nullopt;
    }

    static constexpr base::Optional<WarningReason> ShowNoWarning() {
      return base::nullopt;
    }

    static Decision UseNormalUiAndShowNoWarning();

    // The reason for showing the quiet UI, or `base::nullopt` if the normal UI
    // should be used.
    base::Optional<QuietUiReason> quiet_ui_reason;

    // The reason for printing a warning to the console, or `base::nullopt` if
    // no warning should be printed.
    base::Optional<WarningReason> warning_reason;
  };

  using DecisionMadeCallback = base::OnceCallback<void(const Decision&)>;

  virtual ~NotificationPermissionUiSelector() {}

  // Determines whether animations should be suppressed because we're very
  // confident the user does not want notifications (e.g. they're abusive).
  static bool ShouldSuppressAnimation(QuietUiReason reason);

  // Determines the UI to use for the given |request|, and invokes |callback|
  // when done, either synchronously or asynchronously. The |callback| is
  // guaranteed never to be invoked after |this| goes out of scope. Only one
  // request is supported at a time.
  virtual void SelectUiToUse(PermissionRequest* request,
                             DecisionMadeCallback callback) = 0;

  // Cancel the pending request, if any. After this, the |callback| is
  // guaranteed not to be invoked anymore, and another call to SelectUiToUse()
  // can be issued.
  virtual void Cancel() {}
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_NOTIFICATION_PERMISSION_UI_SELECTOR_H_
