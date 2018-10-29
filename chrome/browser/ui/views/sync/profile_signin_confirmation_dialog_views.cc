// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/profile_signin_confirmation_dialog_views.h"

#include <stddef.h>

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

ProfileSigninConfirmationDialogViews::ProfileSigninConfirmationDialogViews(
    Browser* browser,
    const std::string& username,
    std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate)
    : browser_(browser),
      username_(username),
      delegate_(std::move(delegate)),
      prompt_for_new_profile_(true) {
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::PROFILE_SIGNIN_CONFIRMATION);
}

ProfileSigninConfirmationDialogViews::~ProfileSigninConfirmationDialogViews() {}

// static
void ProfileSigninConfirmationDialogViews::ShowDialog(
    Browser* browser,
    Profile* profile,
    const std::string& username,
    std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate) {
  // Hides the new avatar bubble if it is currently shown. The new avatar bubble
  // should be automatically closed when it loses focus. However on windows the
  // profile signin confirmation dialog is not modal yet thus it does not take
  // away focus, thus as a temporary workaround we need to manually close the
  // bubble.
  // TODO(guohui): removes the workaround once the profile confirmation dialog
  // is fixed.
  ProfileChooserView::Hide();

  ProfileSigninConfirmationDialogViews* dialog =
      new ProfileSigninConfirmationDialogViews(browser, username,
                                               std::move(delegate));
  ui::CheckShouldPromptForNewProfile(
      profile,
      // This callback is guaranteed to be invoked, and once it is, the dialog
      // owns itself.
      base::Bind(&ProfileSigninConfirmationDialogViews::Show,
                 base::Unretained(dialog)));
}

void ProfileSigninConfirmationDialogViews::Show(bool prompt_for_new_profile) {
  prompt_for_new_profile_ = prompt_for_new_profile;
  constrained_window::CreateBrowserModalDialogViews(
      this, browser_->window()->GetNativeWindow())->Show();
}

base::string16 ProfileSigninConfirmationDialogViews::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_ENTERPRISE_SIGNIN_TITLE);
}

base::string16 ProfileSigninConfirmationDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    // If we're giving the option to create a new profile, then OK is
    // "Create new profile".  Otherwise it is "Continue signin".
    return l10n_util::GetStringUTF16(
        prompt_for_new_profile_ ?
            IDS_ENTERPRISE_SIGNIN_CREATE_NEW_PROFILE :
            IDS_ENTERPRISE_SIGNIN_CONTINUE);
  }
  return l10n_util::GetStringUTF16(IDS_ENTERPRISE_SIGNIN_CANCEL);
}

int ProfileSigninConfirmationDialogViews::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_NONE;
}

views::View* ProfileSigninConfirmationDialogViews::CreateExtraView() {
  if (!prompt_for_new_profile_)
    return nullptr;

  const base::string16 continue_signin_text =
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_SIGNIN_CONTINUE);
  return views::MdTextButton::CreateSecondaryUiButton(this,
                                                      continue_signin_text);
}

bool ProfileSigninConfirmationDialogViews::Accept() {
  if (delegate_) {
    if (prompt_for_new_profile_)
      delegate_->OnSigninWithNewProfile();
    else
      delegate_->OnContinueSignin();
    delegate_ = nullptr;
  }
  return true;
}

bool ProfileSigninConfirmationDialogViews::Cancel() {
  if (delegate_) {
    delegate_->OnCancelSignin();
    delegate_ = nullptr;
  }
  return true;
}

ui::ModalType ProfileSigninConfirmationDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void ProfileSigninConfirmationDialogViews::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  views::DialogDelegateView::ViewHierarchyChanged(details);
  if (!details.is_add || details.child != this)
    return;

  const SkColor kPromptBarBackgroundColor =
      ui::GetSigninConfirmationPromptBarColor(
          GetNativeTheme(), ui::kSigninConfirmationPromptBarBackgroundAlpha);

  // Create business icon.
  int business_icon_size = 20;
  views::ImageView* business_icon = new views::ImageView();
  business_icon->SetImage(gfx::CreateVectorIcon(gfx::IconDescription(
      vector_icons::kBusinessIcon, business_icon_size, gfx::kChromeIconGrey,
      base::TimeDelta(), gfx::kNoneIcon)));

  // Create the prompt label.
  size_t offset;
  const base::string16 domain =
      base::ASCIIToUTF16(gaia::ExtractDomainName(username_));
  const base::string16 username = base::ASCIIToUTF16(username_);
  const base::string16 prompt_text =
      l10n_util::GetStringFUTF16(
          IDS_ENTERPRISE_SIGNIN_ALERT,
          domain, &offset);
  views::StyledLabel* prompt_label = new views::StyledLabel(prompt_text, this);
  prompt_label->SetDisplayedOnBackgroundColor(kPromptBarBackgroundColor);

  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.text_style = STYLE_EMPHASIZED;
  prompt_label->AddStyleRange(
      gfx::Range(offset, offset + domain.size()), bold_style);

  // Create the prompt bar.
  views::View* prompt_bar = new views::View;
  prompt_bar->SetBorder(views::CreateSolidSidedBorder(
      1, 0, 1, 0,
      ui::GetSigninConfirmationPromptBarColor(
          GetNativeTheme(), ui::kSigninConfirmationPromptBarBorderAlpha)));
  prompt_bar->SetBackground(
      views::CreateSolidBackground(kPromptBarBackgroundColor));

  // Create the explanation label.
  std::vector<size_t> offsets;
  const base::string16 learn_more_text =
      l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  const base::string16 signin_explanation_text =
      l10n_util::GetStringFUTF16(prompt_for_new_profile_ ?
          IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITH_PROFILE_CREATION :
          IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITHOUT_PROFILE_CREATION,
          username, learn_more_text, &offsets);
  views::StyledLabel* explanation_label =
      new views::StyledLabel(signin_explanation_text, this);
  explanation_label->AddStyleRange(
      gfx::Range(offsets[1], offsets[1] + learn_more_text.size()),
      views::StyledLabel::RangeStyleInfo::CreateForLink());

  // Layout the components.
  const gfx::Insets content_insets =
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::CONTROL, views::TEXT);
  // The prompt bar needs to go to the edge of the dialog, so remove horizontal
  // insets.
  SetBorder(views::CreateEmptyBorder(content_insets.top(), 0,
                                     content_insets.bottom(), 0));
  views::GridLayout* dialog_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));

  // Use GridLayout inside the prompt bar because StyledLabel requires it.
  views::GridLayout* prompt_layout = prompt_bar->SetLayoutManager(
      std::make_unique<views::GridLayout>(prompt_bar));
  prompt_bar->SetBorder(
      views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_DIALOG_SUBSECTION)));
  constexpr int kPromptBarColumnSetId = 0;
  auto* prompt_columnset = prompt_layout->AddColumnSet(kPromptBarColumnSetId);
  prompt_columnset->AddColumn(
      views::GridLayout::FILL, views::GridLayout::CENTER,
      views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0, 0);
  prompt_columnset->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING));
  prompt_columnset->AddColumn(views::GridLayout::FILL,
                              views::GridLayout::CENTER, 1.0,
                              views::GridLayout::USE_PREF, 0, 0);

  prompt_layout->StartRow(views::GridLayout::kFixedSize, kPromptBarColumnSetId);
  prompt_layout->AddView(business_icon);
  prompt_layout->AddView(prompt_label);

  // Use a column set with no padding.
  dialog_layout->AddColumnSet(0)->AddColumn(views::GridLayout::FILL,
                                            views::GridLayout::FILL, 1.0,
                                            views::GridLayout::USE_PREF, 0, 0);
  dialog_layout->StartRow(views::GridLayout::kFixedSize, 0);
  dialog_layout->AddView(
      prompt_bar, 1, 1,
      views::GridLayout::FILL, views::GridLayout::FILL, 0, 0);

  // Use a new column set for the explanation label so we can add padding.
  dialog_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                               content_insets.top());
  constexpr int kExplanationColumnSetId = 1;
  views::ColumnSet* explanation_columns =
      dialog_layout->AddColumnSet(kExplanationColumnSetId);
  explanation_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        content_insets.left());
  explanation_columns->AddColumn(views::GridLayout::FILL,
                                 views::GridLayout::FILL, 1.0,
                                 views::GridLayout::USE_PREF, 0, 0);
  explanation_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        content_insets.right());
  dialog_layout->StartRow(views::GridLayout::kFixedSize,
                          kExplanationColumnSetId);
  const int kPreferredWidth = 440;
  dialog_layout->AddView(explanation_label, 1, 1, views::GridLayout::FILL,
                         views::GridLayout::FILL, kPreferredWidth,
                         explanation_label->GetHeightForWidth(kPreferredWidth));
}

void ProfileSigninConfirmationDialogViews::WindowClosing() {
  Cancel();
}

void ProfileSigninConfirmationDialogViews::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  NavigateParams params(
      browser_, GURL("https://support.google.com/chromebook/answer/1331549"),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);
}

void ProfileSigninConfirmationDialogViews::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(prompt_for_new_profile_);
  if (delegate_) {
    delegate_->OnContinueSignin();
    delegate_ = nullptr;
  }
  GetWidget()->Close();
}
