// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

class PasswordCrossDomainConfirmationPopupView;

// The controller of the cross domain usage confirmation popup. It provides API
// to Show/Hide the popup and get the user's decision (via a callback).
class PasswordCrossDomainConfirmationPopupController
    : public autofill::AutofillPopupViewDelegate,
      public content::WebContentsObserver {
 public:
  explicit PasswordCrossDomainConfirmationPopupController(
      content::WebContents* web_contents);
  PasswordCrossDomainConfirmationPopupController(
      const PasswordCrossDomainConfirmationPopupController&) = delete;
  PasswordCrossDomainConfirmationPopupController& operator=(
      const PasswordCrossDomainConfirmationPopupController&) = delete;
  ~PasswordCrossDomainConfirmationPopupController() override;

  // autofill::AutofillPopupViewDelegate:
  void Hide(autofill::PopupHidingReason reason) override;
  void ViewDestroyed() override;
  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  base::i18n::TextDirection GetElementTextDirection() const override;

  // Creates and shows a popup pointing to `element_bounds` and presenting
  // a message regarding cross domain password usage. `domain` is the domain
  // of the current web site the popup is triggered on. `password_origin` is
  // the name of the place where the password was originally created, it can
  // be the domain of the web site or the name of the Android application.
  // `confirmation_callback` is called if the user confirms the action, if
  // the user cancels it, the popup is silently hidden.
  // If the popup is already shown, it gets hidden and a new one shows up.
  void Show(const gfx::RectF& element_bounds,
            base::i18n::TextDirection text_direction,
            const GURL& domain,
            const std::u16string& password_origin,
            base::OnceClosure confirmation_callback);

 private:
  void HideImpl();
  bool OverlapsWithPictureInPictureWindow() const;

  // Handles the confirmation response from the view.
  void OnConfirm();

  // Handles the cancel response from the view.
  void OnCancel();

  gfx::RectF element_bounds_;
  base::i18n::TextDirection text_direction_;
  base::OnceClosure confirmation_callback_;

  base::WeakPtr<PasswordCrossDomainConfirmationPopupView> view_;

  std::optional<autofill::AutofillPopupHideHelper> popup_hide_helper_;

  base::WeakPtrFactory<PasswordCrossDomainConfirmationPopupController>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_H_
