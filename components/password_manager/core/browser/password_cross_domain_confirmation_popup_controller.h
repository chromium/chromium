// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"

namespace password_manager {

// The controller of the cross domain usage confirmation popup. It provides API
// to Show/Hide the popup and get the user's decision (via a callback).
class PasswordCrossDomainConfirmationPopupController {
 public:
  // The result of user interaction with the bubble. Used for metrics only.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CrossDomainPasswordFillingConfirmation {
    // Cross-domain password filling was confirmed by the user.
    kConfirmed = 0,
    // Cross-domain password filling was explicitly canceled (the cancel-like
    // button was clicked) by the user.
    kCanceled = 1,
    // There was no interaction with the bubble controls, but something else
    // triggered bubble closing, these could be events like scrolling of
    // the page or switching to another app.
    kIgnored = 2,
    kMaxValue = kIgnored,
  };

  virtual ~PasswordCrossDomainConfirmationPopupController() = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_H_
