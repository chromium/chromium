// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_upgrade_bubble_view.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

class PasskeyUpgradeBubbleController : public PasswordBubbleControllerBase {
 public:
  PasskeyUpgradeBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      password_manager::metrics_util::UIDisplayDisposition display_disposition,
      std::string passkey_rp_id)
      : PasswordBubbleControllerBase(delegate, display_disposition),
        passkey_rp_id_(std::move(passkey_rp_id)) {}

  ~PasskeyUpgradeBubbleController() override { OnBubbleClosing(); }

  void OnManagePasswordClicked() {
    if (delegate_) {
      delegate_->NavigateToPasswordDetailsPageInPasswordManager(
          passkey_rp_id_,
          password_manager::ManagePasswordsReferrer::kPasskeyUpgradeBubble);
    }
  }

  // PasswordBubbleControllerBase:
  std::u16string GetTitle() const override {
    // TODO: crbug.com/377758786 - i18n
    return u"Passkey created (UNTRANSLATED)";
  }

 private:
  // PasswordBubbleControllerBase:
  void ReportInteractions() override {
    // TODO: crbug.com/377758786 - Log metrics
  }

  std::string passkey_rp_id_;
};

PasskeyUpgradeBubbleView::PasskeyUpgradeBubbleView(
    content::WebContents* web_contents,
    views::View* anchor,
    DisplayReason display_reason,
    std::string passkey_rp_id)
    : PasswordBubbleViewBase(web_contents,
                             anchor,
                             /*easily_dismissable=*/true),
      controller_(std::make_unique<PasskeyUpgradeBubbleController>(
          PasswordsModelDelegateFromWebContents(web_contents),
          display_reason == DisplayReason::AUTOMATIC
              ? password_manager::metrics_util::AUTOMATIC_PASSKEY_UPGRADE_BUBBLE
              : password_manager::metrics_util::MANUAL_PASSKEY_UPGRADE_BUBBLE,
          passkey_rp_id)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetShowIcon(true);
  SetTitle(controller_->GetTitle());
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // TODO: crbug.com/377758786 - i18n, add link to GPM settings toggle
  std::u16string text = base::StrCat(
      {base::UTF8ToUTF16(passkey_rp_id),
       u" made your account safer by creating a passkey for you. To change "
       u"this, visit Google Password Manager > Settings. (UNTRANSLATED)"});
  AddChildView(views::Builder<views::StyledLabel>()
                   .SetText(std::move(text))
                   .SetTextContext(views::style::CONTEXT_BUBBLE_FOOTER)
                   .SetDefaultTextStyle(views::style::STYLE_SECONDARY)
                   .Build());

  manage_passkeys_button_ = SetExtraView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          [](PasskeyUpgradeBubbleView* view) {
            view->controller_->OnManagePasswordClicked();
          },
          base::Unretained(this)),
      u"Manage Passkeys (UNTRANSLATED)"));
  manage_passkeys_button_->SetStyle(ui::ButtonStyle::kTonal);
}

PasskeyUpgradeBubbleView::~PasskeyUpgradeBubbleView() = default;

views::MdTextButton*
PasskeyUpgradeBubbleView::manage_passkeys_button_for_testing() {
  return manage_passkeys_button_;
}

PasswordBubbleControllerBase* PasskeyUpgradeBubbleView::GetController() {
  return controller_.get();
}

const PasswordBubbleControllerBase* PasskeyUpgradeBubbleView::GetController()
    const {
  return controller_.get();
}

ui::ImageModel PasskeyUpgradeBubbleView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

BEGIN_METADATA(PasskeyUpgradeBubbleView)
END_METADATA
