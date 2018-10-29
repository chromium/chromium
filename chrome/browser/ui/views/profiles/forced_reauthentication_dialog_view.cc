// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/forced_reauthentication_dialog_view.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/i18n/message_formatter.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/signin/core/browser/signin_manager.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

// Refresh title of the dialog every second.
constexpr int kRefreshTitleTimer = 1;

// If browser windows are going to be closed soon, close browser window before
// showing sign in dialog because there might not be enough time for user to
// finish sign in.
constexpr int kCloseDirectlyTimer = 60;

void Signout(identity::IdentityManager* identity_manager) {
  identity_manager->ClearPrimaryAccount(
      identity::IdentityManager::ClearAccountTokensAction::kDefault,
      signin_metrics::AUTHENTICATION_FAILED_WITH_FORCE_SIGNIN,
      signin_metrics::SignoutDelete::KEEPING);
}

bool IsMatchingBrowser(Browser* browser, Profile* profile) {
  return browser->profile()->GetOriginalProfile() ==
             profile->GetOriginalProfile() &&
         !browser->tab_strip_model()->empty() && browser->window()->IsVisible();
}

// Find a browser that is associated with |profile| to show the dialog for
// Sign out warning.
Browser* FindBrowserWithProfile(Profile* profile) {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (browser && IsMatchingBrowser(browser, profile))
    return browser;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (IsMatchingBrowser(browser, profile)) {
      return browser;
    }
  }
  return nullptr;
}

// PromptLabel overrides the default insets of StyledLabel.
class PromptLabel : public views::StyledLabel {
 public:
  PromptLabel(const base::string16& text, views::StyledLabelListener* listener)
      : views::StyledLabel(text, listener) {}

  gfx::Insets GetInsets() const override {
    return ChromeLayoutProvider::Get()->GetInsetsMetric(
        views::INSETS_DIALOG_SUBSECTION);
  }
};

}  // namespace

// ForcedReauthenticationDialogView

ForcedReauthenticationDialogView::ForcedReauthenticationDialogView(
    Browser* browser,
    identity::IdentityManager* identity_manager,
    base::TimeDelta countdown_duration)
    : browser_(browser),
      identity_manager_(identity_manager),
      desired_close_time_(base::TimeTicks::Now() + countdown_duration),
      weak_factory_(this) {
  constrained_window::CreateBrowserModalDialogViews(
      this, browser->window()->GetNativeWindow())
      ->Show();
  browser->window()->FlashFrame(true);
  browser->window()->Activate();
}

ForcedReauthenticationDialogView::~ForcedReauthenticationDialogView() {}

// static
ForcedReauthenticationDialogView* ForcedReauthenticationDialogView::ShowDialog(
    Profile* profile,
    identity::IdentityManager* identity_manager,
    base::TimeDelta countdown_duration) {
  Browser* browser = FindBrowserWithProfile(profile);
  if (browser == nullptr) {  // If there is no browser, we can just sign
                             // out profile directly.
    Signout(identity_manager);
    return nullptr;
  }

  return new ForcedReauthenticationDialogView(browser, identity_manager,
                                              countdown_duration);
}

bool ForcedReauthenticationDialogView::Accept() {
  if (GetTimeRemaining() < base::TimeDelta::FromSeconds(kCloseDirectlyTimer)) {
    Signout(identity_manager_);
  } else {
    browser_->signin_view_controller()->ShowSignin(
        profiles::BubbleViewMode::BUBBLE_VIEW_MODE_GAIA_REAUTH, browser_,
        signin_metrics::AccessPoint::ACCESS_POINT_FORCE_SIGNIN_WARNING);
  }
  return true;
}

bool ForcedReauthenticationDialogView::Cancel() {
  return true;
}

void ForcedReauthenticationDialogView::WindowClosing() {
  refresh_timer_.Stop();
}

base::string16 ForcedReauthenticationDialogView::GetWindowTitle() const {
  base::TimeDelta time_left = GetTimeRemaining();
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_FORCE_SIGNOUT_TITLE),
      time_left.InMinutes(), time_left.InSeconds() % 60);
}

base::string16 ForcedReauthenticationDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    return l10n_util::GetStringUTF16(
        IDS_ENTERPRISE_FORCE_SIGNOUT_CLOSE_CONFIRM);
  }
  return l10n_util::GetStringUTF16(IDS_ENTERPRISE_FORCE_SIGNOUT_CLOSE_DELAY);
}

ui::ModalType ForcedReauthenticationDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void ForcedReauthenticationDialogView::AddedToWidget() {
  const SkColor prompt_bar_background_color =
      GetSigninConfirmationPromptBarColor(
          GetNativeTheme(), ui::kSigninConfirmationPromptBarBackgroundAlpha);
  // Create the prompt label.
  size_t offset;
  std::string email = identity_manager_->GetPrimaryAccountInfo().email;
  const base::string16 domain =
      base::ASCIIToUTF16(gaia::ExtractDomainName(email));
  const base::string16 prompt_text =
      l10n_util::GetStringFUTF16(IDS_ENTERPRISE_SIGNIN_ALERT, domain, &offset);

  // Create the prompt label.
  PromptLabel* prompt_label = new PromptLabel(prompt_text, nullptr);
  prompt_label->SetDisplayedOnBackgroundColor(prompt_bar_background_color);

  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.text_style = STYLE_EMPHASIZED;
  prompt_label->AddStyleRange(gfx::Range(offset, offset + domain.size()),
                              bold_style);

  prompt_label->SetBorder(views::CreateSolidSidedBorder(
      1, 0, 1, 0,
      ui::GetSigninConfirmationPromptBarColor(
          GetNativeTheme(), ui::kSigninConfirmationPromptBarBorderAlpha)));
  prompt_label->SetBackground(
      views::CreateSolidBackground(prompt_bar_background_color));

  // Create the explanation label.
  base::string16 signin_explanation_text;
  base::string16 close_warning;
  close_warning = l10n_util::GetStringUTF16(
      IDS_ENTERPRISE_FORCE_SIGNOUT_ADDITIONAL_EXPLANATION);
  if (email.empty()) {
    signin_explanation_text = l10n_util::GetStringFUTF16(
        IDS_ENTERPRISE_FORCE_SIGNOUT_EXPLANATION_WITHOUT_USER_NAME,
        close_warning);
  } else {
    signin_explanation_text =
        l10n_util::GetStringFUTF16(IDS_ENTERPRISE_FORCE_SIGNOUT_EXPLANATION,
                                   base::ASCIIToUTF16(email), close_warning);
  }
  views::StyledLabel* explanation_label =
      new views::StyledLabel(signin_explanation_text, nullptr);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  // Layout the components.
  const gfx::Insets dialog_insets =
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT);
  SetBorder(views::CreateEmptyBorder(dialog_insets.top(), 0,
                                     dialog_insets.bottom(), 0));
  views::GridLayout* dialog_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));

  // Use a column set with no padding.
  dialog_layout->AddColumnSet(0)->AddColumn(views::GridLayout::FILL,
                                            views::GridLayout::FILL, 1.0,
                                            views::GridLayout::USE_PREF, 0, 0);
  dialog_layout->StartRow(views::GridLayout::kFixedSize, 0);
  dialog_layout->AddView(prompt_label, 1, 1, views::GridLayout::FILL,
                         views::GridLayout::FILL, 0, 0);

  // Use a new column set for the explanation label so we can add padding.
  dialog_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                               dialog_insets.top());
  views::ColumnSet* explanation_columns = dialog_layout->AddColumnSet(1);

  explanation_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        dialog_insets.left());
  explanation_columns->AddColumn(views::GridLayout::FILL,
                                 views::GridLayout::FILL, 1.0,
                                 views::GridLayout::USE_PREF, 0, 0);
  explanation_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        dialog_insets.right());
  dialog_layout->StartRow(views::GridLayout::kFixedSize, 1);
  const int kPreferredWidth = 440;
  dialog_layout->AddView(explanation_label, 1, 1, views::GridLayout::FILL,
                         views::GridLayout::FILL, kPreferredWidth,
                         explanation_label->GetHeightForWidth(kPreferredWidth));
  refresh_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromSeconds(kRefreshTitleTimer), this,
                       &ForcedReauthenticationDialogView::OnCountDown);
}

void ForcedReauthenticationDialogView::CloseDialog() {
  GetWidget()->Close();
}

void ForcedReauthenticationDialogView::OnCountDown() {
  if (desired_close_time_ <= base::TimeTicks::Now()) {
    Cancel();
    GetWidget()->Close();
  }
  GetWidget()->UpdateWindowTitle();
}

base::TimeDelta ForcedReauthenticationDialogView::GetTimeRemaining() const {
  base::TimeTicks now = base::TimeTicks::Now();
  if (desired_close_time_ <= now)
    return base::TimeDelta();
  return desired_close_time_ - now;
}

// ForcedReauthenticationDialogImpl

ForcedReauthenticationDialogImpl::ForcedReauthenticationDialogImpl() {}
ForcedReauthenticationDialogImpl::~ForcedReauthenticationDialogImpl() {
  if (dialog_view_)
    dialog_view_->CloseDialog();
}

void ForcedReauthenticationDialogImpl::ShowDialog(
    Profile* profile,
    identity::IdentityManager* identity_manager,
    base::TimeDelta countdown_duration) {
  dialog_view_ = ForcedReauthenticationDialogView::ShowDialog(
                     profile, identity_manager, countdown_duration)
                     ->AsWeakPtr();
}

// ForcedReauthenticationDialog

// static
std::unique_ptr<ForcedReauthenticationDialog>
ForcedReauthenticationDialog::Create() {
  return std::make_unique<ForcedReauthenticationDialogImpl>();
}
