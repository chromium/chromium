// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/view_class_properties.h"

ManagePasswordsIconViews::ManagePasswordsIconViews(
    CommandUpdater* updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(updater,
                         IDC_MANAGE_PASSWORDS_FOR_PAGE,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "ManagePasswords") {
  // Password icon should not be mirrored in RTL.
  image()->SetFlipCanvasOnPaintForRTLUI(false);
  SetProperty(views::kElementIdentifierKey, kPasswordsOmniboxKeyIconElementId);
  SetAccessibilityProperties(/*role*/ std::nullopt,
                             GetTextForTooltipAndAccessibleName());
}

ManagePasswordsIconViews::~ManagePasswordsIconViews() = default;

void ManagePasswordsIconViews::SetState(password_manager::ui::State state) {
  if (state_ == state)
    return;
  // If there is an opened bubble for the current icon it should go away.
  PasswordBubbleViewBase::CloseCurrentBubble();
  state_ = state;
  UpdateUiForState();
  SetAccessibleName(GetTextForTooltipAndAccessibleName());
}

void ManagePasswordsIconViews::UpdateUiForState() {
  if (state_ == password_manager::ui::INACTIVE_STATE) {
    SetVisible(false);
    return;
  }

  SetVisible(true);

  // We may be about to automatically pop up a passwords bubble.
  // Force layout of the icon's parent now; the bubble will be incorrectly
  // positioned otherwise, as the icon won't have been drawn into position.
  parent()->Layout();
}

views::BubbleDialogDelegate* ManagePasswordsIconViews::GetBubble() const {
  return PasswordBubbleViewBase::manage_password_bubble();
}

void ManagePasswordsIconViews::UpdateImpl() {
  if (!GetWebContents())
    return;

  ManagePasswordsUIController::FromWebContents(GetWebContents())
      ->UpdateIconAndBubbleState(this);
}

void ManagePasswordsIconViews::OnExecuting(
    PageActionIconView::ExecuteSource source) {}

bool ManagePasswordsIconViews::OnMousePressed(const ui::MouseEvent& event) {
  bool result = PageActionIconView::OnMousePressed(event);
  PasswordBubbleViewBase::CloseCurrentBubble();
  return result;
}

const gfx::VectorIcon& ManagePasswordsIconViews::GetVectorIcon() const {
  return OmniboxFieldTrial::IsChromeRefreshIconsEnabled()
             ? kKeyOpenChromeRefreshIcon
             : kKeyIcon;
}

std::u16string ManagePasswordsIconViews::GetTextForTooltipAndAccessibleName()
    const {
  switch (state_) {
    case password_manager::ui::INACTIVE_STATE:
    case password_manager::ui::SAVE_CONFIRMATION_STATE:
    case password_manager::ui::UPDATE_CONFIRMATION_STATE:
    case password_manager::ui::CREDENTIAL_REQUEST_STATE:
    case password_manager::ui::AUTO_SIGNIN_STATE:
    case password_manager::ui::WILL_DELETE_UNSYNCED_ACCOUNT_PASSWORDS_STATE:
    case password_manager::ui::MANAGE_STATE:
    case password_manager::ui::PASSWORD_UPDATED_SAFE_STATE:
    case password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX:
      return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE);
    case password_manager::ui::PENDING_PASSWORD_UPDATE_STATE:
    case password_manager::ui::PENDING_PASSWORD_STATE:
    case password_manager::ui::GENERATED_PASSWORD_CONFIRMATION_STATE:
      return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_SAVE);
    case password_manager::ui::CAN_MOVE_PASSWORD_TO_ACCOUNT_STATE:
      return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_MOVE);
    case password_manager::ui::BIOMETRIC_AUTHENTICATION_FOR_FILLING_STATE:
    case password_manager::ui::BIOMETRIC_AUTHENTICATION_CONFIRMATION_STATE:
      return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_PROTECT);
    case password_manager::ui::NOTIFY_RECEIVED_SHARED_CREDENTIALS:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_TOOLTIP_SHARED_NOTIFICATION);
    case password_manager::ui::KEYCHAIN_ERROR_STATE:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_TOOLTIP_KEYCHAIN_ERROR);
  }
  NOTREACHED_NORETURN();
}

void ManagePasswordsIconViews::AboutToRequestFocusFromTabTraversal(
    bool reverse) {
  if (IsBubbleShowing())
    PasswordBubbleViewBase::ActivateBubble();
}

BEGIN_METADATA(ManagePasswordsIconViews, PageActionIconView)
END_METADATA
