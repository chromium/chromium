// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_page_action_controller.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"

namespace {

const gfx::VectorIcon& GetVectorIconForState(password_manager::ui::State state,
                                             bool is_blocklisted) {
  if (is_blocklisted &&
      base::FeatureList::IsEnabled(features::kSavePasswordsContextualUi)) {
    return vector_icons::kPasswordManagerOffIcon;
  }
  return vector_icons::kPasswordManagerIcon;
}

}  // namespace

// static
std::u16string
ManagePasswordsPageActionController::GetManagePasswordsTooltipText(
    password_manager::ui::State state,
    bool is_blocklisted) {
  std::u16string result;
  switch (state) {
    case password_manager::ui::INACTIVE_STATE:
    case password_manager::ui::SAVE_CONFIRMATION_STATE:
    case password_manager::ui::UPDATE_CONFIRMATION_STATE:
    case password_manager::ui::CREDENTIAL_REQUEST_STATE:
    case password_manager::ui::AUTO_SIGNIN_STATE:
    case password_manager::ui::MANAGE_STATE:
    case password_manager::ui::PASSWORD_UPDATED_SAFE_STATE:
    case password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX:
    case password_manager::ui::PASSWORD_CHANGE_STATE:
    case password_manager::ui::PASSKEY_SAVED_CONFIRMATION_STATE:
    case password_manager::ui::PASSKEY_DELETED_CONFIRMATION_STATE:
    case password_manager::ui::PASSKEY_UPDATED_CONFIRMATION_STATE:
    case password_manager::ui::PASSKEY_NOT_ACCEPTED_STATE:
    case password_manager::ui::PASSKEY_UPGRADE_STATE:
      result = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE);
      break;
    case password_manager::ui::PENDING_PASSWORD_UPDATE_STATE:
    case password_manager::ui::PENDING_PASSWORD_STATE:
    case password_manager::ui::GENERATED_PASSWORD_CONFIRMATION_STATE:
      result = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_SAVE);
      break;
    case password_manager::ui::MOVE_CREDENTIAL_AFTER_LOG_IN_STATE:
    case password_manager::ui::MOVE_CREDENTIAL_FROM_MANAGE_BUBBLE_STATE:
      result = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_MOVE);
      break;
    case password_manager::ui::BIOMETRIC_AUTHENTICATION_FOR_FILLING_STATE:
    case password_manager::ui::BIOMETRIC_AUTHENTICATION_CONFIRMATION_STATE:
      result = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_PROTECT);
      break;
    case password_manager::ui::NOTIFY_RECEIVED_SHARED_CREDENTIALS:
      result = l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_TOOLTIP_SHARED_NOTIFICATION);
      break;
    case password_manager::ui::KEYCHAIN_ERROR_STATE:
      result = l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_TOOLTIP_KEYCHAIN_ERROR);
      break;
  }
  if (is_blocklisted &&
      base::FeatureList::IsEnabled(features::kSavePasswordsContextualUi)) {
    result += u" - " +
              l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_BLOCKED);
  }
  return result;
}

ManagePasswordsPageActionController::ManagePasswordsPageActionController(
    page_actions::PageActionController& page_action_controller)
    : page_action_controller_(page_action_controller) {
  CHECK(IsPageActionMigrated(PageActionIconType::kManagePasswords));
}

ManagePasswordsPageActionController::~ManagePasswordsPageActionController() =
    default;

void ManagePasswordsPageActionController::UpdateVisibility(
    password_manager::ui::State state,
    bool is_blocklisted,
    ManagePasswordsUIController& passwords_ui_controller,
    actions::ActionItem& passwords_action_item) {
  bool should_be_visible = state != password_manager::ui::INACTIVE_STATE;
  if (should_be_visible) {
    page_action_controller_->Show(kActionShowPasswordsBubbleOrPage);
    const gfx::VectorIcon& icon = GetVectorIconForState(state, is_blocklisted);
    // Checks for if the bubble is showing or is about to be showing as there
    // should be no tooltip if bubble is open.
    bool bubble_is_or_will_be_showing =
        (PasswordBubbleViewBase::manage_password_bubble() &&
         PasswordBubbleViewBase::manage_password_bubble()
             ->GetWidget()
             ->IsVisible()) ||
        passwords_ui_controller.IsAutomaticallyOpeningBubble();
    std::u16string tooltip =
        bubble_is_or_will_be_showing
            ? std::u16string()
            : GetManagePasswordsTooltipText(state, is_blocklisted);
    page_action_controller_->OverrideImage(
        kActionShowPasswordsBubbleOrPage, ui::ImageModel::FromVectorIcon(icon));
    page_action_controller_->OverrideTooltip(kActionShowPasswordsBubbleOrPage,
                                             tooltip);
    page_action_controller_->OverrideAccessibleName(
        kActionShowPasswordsBubbleOrPage, tooltip);
  } else {
    page_action_controller_->Hide(kActionShowPasswordsBubbleOrPage);
  }
  // Updates the underline indicator for the pinned toolbar button.
  passwords_action_item.SetProperty(kActionItemUnderlineIndicatorKey,
                                    should_be_visible);
}
