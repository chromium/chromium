// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_migration_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kHelpCenterUrl[] =
    "https://support.google.com/chrome/answer/185277";

void OnHelpCenterLinkClicked(Browser* browser) {
  browser->OpenGURL(GURL(kHelpCenterUrl),
                    WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

bool IsUserEligibleForDiceMigration(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) ||
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    // The user is not signed in or has sync enabled.
    return false;
  }
  if (!signin::IsImplicitBrowserSigninOrExplicitDisabled(identity_manager,
                                                         profile->GetPrefs())) {
    // The user is not implicitly signed in.
    return false;
  }
  // TODO(crbug.com/399838468): Add more eligibility checks, for example, when
  // was the last time the user was shown the migration dialog.
  return true;
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DiceMigrationService,
                                      kAcceptButtonElementId);

DiceMigrationService::DiceMigrationService(Profile* profile)
    : profile_(profile) {}

DiceMigrationService::~DiceMigrationService() {
  if (dialog_widget_) {
    dialog_widget_observation_.Reset();
    dialog_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

// static
void DiceMigrationService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}

void DiceMigrationService::ShowDiceMigrationOfferDialogIfUserEligible() {
  if (!IsUserEligibleForDiceMigration(profile_) || IsDialogShowing()) {
    return;
  }

  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  if (!browser || !browser->window()) {
    return;
  }

  ui::DialogModelLabel::TextReplacement learn_more_link =
      ui::DialogModelLabel::CreateLink(
          IDS_LEARN_MORE,
          base::BindRepeating(&OnHelpCenterLinkClicked, browser));

  auto description_text = ui::DialogModelLabel::CreateWithReplacement(
      IDS_DICE_MIGRATION_DIALOG_DESCRIPTION, learn_more_link);

  auto builder =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>());
  builder.SetTitle(l10n_util::GetStringUTF16(IDS_DICE_MIGRATION_DIALOG_TITLE));
  builder.AddParagraph(description_text);
  builder.AddOkButton(base::DoNothing(),
                      ui::DialogModel::Button::Params()
                          .SetId(kAcceptButtonElementId)
                          .SetLabel(l10n_util::GetStringUTF16(
                              IDS_DICE_MIGRATION_DIALOG_OK_BUTTON)));
  // TODO(crbug.com/399838468): Refine the dialog behavior.
  builder.DisableCloseOnDeactivate();
  builder.SetIsAlertDialog();
  // TODO(crbug.com/399838468): Add a banner image.

  AvatarToolbarButton* avatar_button =
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetAvatarToolbarButton();
  if (!avatar_button) {
    // Skip showing the dialog if the avatar button is not available.
    return;
  }

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      builder.Build(), avatar_button, views::BubbleBorder::TOP_RIGHT);
  dialog_widget_ = views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  dialog_widget_observation_.Observe(dialog_widget_);
  dialog_widget_->Show();

  // TODO(crbug.com/399838468): Close the dialog when the avatar pill is
  // clicked.
}

bool DiceMigrationService::IsDialogShowing() {
  return dialog_widget_ && !dialog_widget_->IsClosed();
}

views::Widget* DiceMigrationService::GetDialogWidgetForTesting() {
  return dialog_widget_.get();
}

void DiceMigrationService::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(dialog_widget_, widget);
  dialog_widget_observation_.Reset();
  dialog_widget_ = nullptr;
  // TODO(crbug.com/399838468): Add actions for the different close reasons.
  switch (widget->closed_reason()) {
    // Losing focus should not close the dialog.
    case views::Widget::ClosedReason::kLostFocus:
    // No close button in the dialog.
    case views::Widget::ClosedReason::kCancelButtonClicked:
      NOTREACHED();
    case views::Widget::ClosedReason::kAcceptButtonClicked:
    case views::Widget::ClosedReason::kUnspecified:
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      break;
  }
}
