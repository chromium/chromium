// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_EMAIL_VERIFICATION_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_EMAIL_VERIFICATION_POPUP_VIEW_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "net/base/schemeful_site.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/input_event_activation_protector.h"

namespace ui {
class Event;
}

namespace views {
class Widget;
}

namespace autofill {

class EmailVerificationPopupController;

// A popup that asks the user for permission to automatically verify their
// email address with an identity provider.
// Similar to PasswordCrossDomainConfirmationPopupViewViews.
class EmailVerificationPopupView : public autofill::PopupBaseView {
  METADATA_HEADER(EmailVerificationPopupView, autofill::PopupBaseView)
 public:
  enum class PopupViewId {
    kNone = 0,
    kConfirmButton,
    kCancelButton,
  };

  static base::WeakPtr<EmailVerificationPopupView> Show(
      base::WeakPtr<EmailVerificationPopupController> controller,
      views::Widget* parent_widget,
      const net::SchemefulSite& issuer_site,
      const std::u16string& email,
      base::OnceCallback<void(bool)> callback);

  EmailVerificationPopupView(
      base::WeakPtr<EmailVerificationPopupController> controller,
      views::Widget* parent_widget,
      const net::SchemefulSite& issuer_site,
      const std::u16string& email,
      base::OnceCallback<void(bool)> callback);
  EmailVerificationPopupView(const EmailVerificationPopupView&) = delete;
  EmailVerificationPopupView& operator=(const EmailVerificationPopupView&) =
      delete;

  ~EmailVerificationPopupView() override;

  // PopupBaseView:
  virtual void Hide();
  virtual void Show();
  virtual bool OverlapsWithPictureInPictureWindow() const;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  base::WeakPtr<EmailVerificationPopupView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnConfirm(const ui::Event& event);
  void OnCancel(const ui::Event& event);

  base::OnceCallback<void(bool)> callback_;
  views::InputEventActivationProtector input_protector_;
  base::WeakPtrFactory<EmailVerificationPopupView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_EMAIL_VERIFICATION_POPUP_VIEW_H_
