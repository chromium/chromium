// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

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

  virtual void Hide(autofill::SuggestionHidingReason reason) = 0;

  // Creates and shows a popup pointing to `element_bounds` and presenting
  // a message regarding cross domain password usage. `domain` is the domain
  // of the current web site the popup is triggered on. `password_domain` is
  // the domain of the web site the password was originally stored on.
  // `confirmation_callback` is called if the user confirms the action, if
  // the user cancels it, the popup is silently hidden.
  // If the popup is already shown, it gets hidden and a new one shows up.
  virtual void Show(const gfx::RectF& element_bounds,
                    base::i18n::TextDirection text_direction,
                    const GURL& domain,
                    const std::u16string& password_origin,
                    base::OnceClosure confirmation_callback) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_H_
