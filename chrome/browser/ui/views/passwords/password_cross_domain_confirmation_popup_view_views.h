// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_VIEW_VIEWS_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/passwords/password_cross_domain_confirmation_popup_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

// The views implementation of the `PasswordCrossDomainConfirmationPopupView`.
class PasswordCrossDomainConfirmationPopupViewViews
    : public PasswordCrossDomainConfirmationPopupView,
      public autofill::PopupBaseView {
  METADATA_HEADER(PasswordCrossDomainConfirmationPopupViewViews,
                  autofill::PopupBaseView)

 public:
  PasswordCrossDomainConfirmationPopupViewViews(
      base::WeakPtr<autofill::AutofillPopupViewDelegate> delegate,
      views::Widget* parent_widget,
      const GURL& domain,
      const std::u16string& password_origin,
      base::OnceClosure confirmation_callback,
      base::OnceClosure cancel_callback);

  PasswordCrossDomainConfirmationPopupViewViews(
      const PasswordCrossDomainConfirmationPopupViewViews&) = delete;
  PasswordCrossDomainConfirmationPopupViewViews& operator=(
      const PasswordCrossDomainConfirmationPopupViewViews&) = delete;
  ~PasswordCrossDomainConfirmationPopupViewViews() override;

  // PasswordCrossDomainConfirmationPopupView:
  void Hide() override;
  bool OverlapsWithPictureInPictureWindow() const override;

  void Show();

  base::WeakPtr<PasswordCrossDomainConfirmationPopupViewViews> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<PasswordCrossDomainConfirmationPopupViewViews>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_VIEW_VIEWS_H_
