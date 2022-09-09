// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crypto_module_password_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget.h"

////////////////////////////////////////////////////////////////////////////////
// CryptoModulePasswordDialogView, public:

CryptoModulePasswordDialogView::CryptoModulePasswordDialogView(
    const std::string& slot_name,
    CryptoModulePasswordReason reason,
    const std::string& hostname,
    CryptoModulePasswordCallback callback)
    : callback_(std::move(callback)) {
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_CRYPTO_MODULE_AUTH_DIALOG_OK_BUTTON_LABEL));
  SetAcceptCallback(base::BindOnce(
      [](CryptoModulePasswordDialogView* dialog) {
        std::move(dialog->callback_)
            .Run(base::UTF16ToUTF8(dialog->password_entry_->GetText()));
      },
      base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      [](CryptoModulePasswordDialogView* dialog) {
        std::move(dialog->callback_).Run(std::string());
      },
      base::Unretained(this)));
  SetModalType(ui::MODAL_TYPE_WINDOW);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl));
  Init(hostname, slot_name, reason);
}

CryptoModulePasswordDialogView::~CryptoModulePasswordDialogView() {
}

////////////////////////////////////////////////////////////////////////////////
// CryptoModulePasswordDialogView, private:

views::View* CryptoModulePasswordDialogView::GetInitiallyFocusedView() {
  return password_entry_;
}

std::u16string CryptoModulePasswordDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CRYPTO_MODULE_AUTH_DIALOG_TITLE);
}

void CryptoModulePasswordDialogView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {}

bool CryptoModulePasswordDialogView::HandleKeyEvent(
    views::Textfield* sender,
    const ui::KeyEvent& keystroke) {
  return false;
}

void CryptoModulePasswordDialogView::Init(const std::string& hostname,
                                          const std::string& slot_name,
                                          CryptoModulePasswordReason reason) {
  // Select an appropriate text for the reason.
  std::string text;
  const std::u16string& hostname16 = base::UTF8ToUTF16(hostname);
  const std::u16string& slot16 = base::UTF8ToUTF16(slot_name);
  switch (reason) {
    case kCryptoModulePasswordCertEnrollment:
      text = l10n_util::GetStringFUTF8(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_TEXT_CERT_ENROLLMENT,
          slot16,
          hostname16);
      break;
    case kCryptoModulePasswordClientAuth:
      text = l10n_util::GetStringFUTF8(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_TEXT_CLIENT_AUTH, slot16, hostname16);
      break;
    case kCryptoModulePasswordListCerts:
      text = l10n_util::GetStringFUTF8(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_TEXT_LIST_CERTS, slot16);
      break;
    case kCryptoModulePasswordCertImport:
      text = l10n_util::GetStringFUTF8(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_TEXT_CERT_IMPORT, slot16);
      break;
    case kCryptoModulePasswordCertExport:
      text = l10n_util::GetStringFUTF8(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_TEXT_CERT_EXPORT, slot16);
      break;
    default:
      NOTREACHED();
  }

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  reason_label_ =
      AddChildView(std::make_unique<views::Label>(base::UTF8ToUTF16(text)));
  reason_label_->SetMultiLine(true);
  reason_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* password_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  password_container->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  password_label_ = password_container->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_PASSWORD_FIELD)));
  password_entry_ =
      password_container->AddChildView(std::make_unique<views::Textfield>());
  password_entry_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  password_entry_->set_controller(this);
  password_entry_->SetAssociatedLabel(password_label_);
  password_container->SetFlexForView(password_entry_, 1);
}

BEGIN_METADATA(CryptoModulePasswordDialogView, views::DialogDelegateView)
END_METADATA

void ShowCryptoModulePasswordDialog(const std::string& slot_name,
                                    bool retry,
                                    CryptoModulePasswordReason reason,
                                    const std::string& hostname,
                                    gfx::NativeWindow parent,
                                    CryptoModulePasswordCallback callback) {
  CryptoModulePasswordDialogView* dialog = new CryptoModulePasswordDialogView(
      slot_name, reason, hostname, std::move(callback));
  views::DialogDelegate::CreateDialogWidget(dialog, nullptr, parent)->Show();
}
