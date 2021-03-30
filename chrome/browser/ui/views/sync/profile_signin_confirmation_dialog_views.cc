// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/profile_signin_confirmation_dialog_views.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
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
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

ProfileSigninConfirmationDialogViews::ProfileSigninConfirmationDialogViews(
    Browser* browser,
    const std::string& username,
    std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate,
    bool prompt_for_new_profile)
    : browser_(browser),
      username_(username),
      delegate_(std::move(delegate)),
      prompt_for_new_profile_(prompt_for_new_profile),
      use_work_profile_wording_(base::FeatureList::IsEnabled(
          features::kSyncConfirmationUpdatedText)) {
  if (use_work_profile_wording_) {
    SetTitle(IDS_ENTERPRISE_SIGNIN_WORK_PROFILE_TITLE);
    // Create business icon.
    int business_icon_size = 20;
    auto business_icon = std::make_unique<views::ImageView>();
    SetIcon(gfx::CreateVectorIcon(
        gfx::IconDescription(vector_icons::kBusinessIcon, business_icon_size,
                             gfx::kChromeIconGrey)));
    SetShowIcon(true);
    SetDefaultButton(ui::DIALOG_BUTTON_OK);
    SetButtonLabel(ui::DIALOG_BUTTON_OK,
                   l10n_util::GetStringUTF16(
                       prompt_for_new_profile_
                           ? IDS_ENTERPRISE_SIGNIN_CREATE_NEW_WORK_PROFILE
                           : IDS_ENTERPRISE_SIGNIN_CONTINUE));
  } else {
    SetShowCloseButton(false);
    SetTitle(IDS_ENTERPRISE_SIGNIN_TITLE);
    SetDefaultButton(ui::DIALOG_BUTTON_NONE);
    SetButtonLabel(
        ui::DIALOG_BUTTON_OK,
        l10n_util::GetStringUTF16(prompt_for_new_profile_
                                      ? IDS_ENTERPRISE_SIGNIN_CREATE_NEW_PROFILE
                                      : IDS_ENTERPRISE_SIGNIN_CONTINUE));
    if (prompt_for_new_profile) {
      SetExtraView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&ProfileSigninConfirmationDialogViews::
                                  ContinueSigninButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_ENTERPRISE_SIGNIN_CONTINUE)));
    }
  }
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_ENTERPRISE_SIGNIN_CANCEL));
  SetModalType(ui::MODAL_TYPE_WINDOW);

  using Delegate = ui::ProfileSigninConfirmationDelegate;
  using DelegateNotifyFn = void (Delegate::*)();
  auto notify_delegate = [](ProfileSigninConfirmationDialogViews* dialog,
                            DelegateNotifyFn fn) {
    if (dialog->delegate_) {
      (dialog->delegate_.get()->*fn)();
      dialog->delegate_.reset();
    }
  };
  SetAcceptCallback(base::BindOnce(notify_delegate, base::Unretained(this),
                                   prompt_for_new_profile_
                                       ? &Delegate::OnSigninWithNewProfile
                                       : &Delegate::OnContinueSignin));
  SetCancelCallback(base::BindOnce(notify_delegate, base::Unretained(this),
                                   &Delegate::OnCancelSignin));

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::PROFILE_SIGNIN_CONFIRMATION);
}

ProfileSigninConfirmationDialogViews::~ProfileSigninConfirmationDialogViews() {}

// static
void ProfileSigninConfirmationDialogViews::Show(
    Browser* browser,
    const std::string& username,
    std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate,
    bool prompt_for_new_profile) {
  auto dialog = std::make_unique<ProfileSigninConfirmationDialogViews>(
      browser, username, std::move(delegate), prompt_for_new_profile);
  constrained_window::CreateBrowserModalDialogViews(
      dialog.release(), browser->window()->GetNativeWindow())
      ->Show();
}

void ProfileSigninConfirmationDialogViews::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  views::DialogDelegateView::ViewHierarchyChanged(details);
  if (!details.is_add || details.child != this)
    return;

  if (use_work_profile_wording_)
    BuildWorkProfileView();
  else
    BuildDefaultView();
}

void ProfileSigninConfirmationDialogViews::BuildDefaultView() {
  DCHECK(!use_work_profile_wording_);
  const SkColor kPromptBarBackgroundColor =
      ui::GetSigninConfirmationPromptBarColor(GetNativeTheme(), 0x0A);

  // Create business icon.
  int business_icon_size = 20;
  auto business_icon = std::make_unique<views::ImageView>();
  business_icon->SetImage(gfx::CreateVectorIcon(gfx::IconDescription(
      vector_icons::kBusinessIcon, business_icon_size, gfx::kChromeIconGrey)));

  // Create the prompt label.
  size_t offset;
  const std::u16string domain =
      base::ASCIIToUTF16(gaia::ExtractDomainName(username_));
  const std::u16string username = base::ASCIIToUTF16(username_);
  const std::u16string prompt_text =
      l10n_util::GetStringFUTF16(IDS_ENTERPRISE_SIGNIN_ALERT, domain, &offset);
  auto prompt_label = std::make_unique<views::StyledLabel>();
  prompt_label->SetText(prompt_text);
  prompt_label->SetDisplayedOnBackgroundColor(kPromptBarBackgroundColor);

  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.text_style = STYLE_EMPHASIZED;
  prompt_label->AddStyleRange(
      gfx::Range(offset, offset + domain.size()), bold_style);

  // Create the prompt bar.
  auto prompt_bar = std::make_unique<views::View>();
  prompt_bar->SetBorder(views::CreateSolidSidedBorder(
      1, 0, 1, 0,
      ui::GetSigninConfirmationPromptBarColor(GetNativeTheme(), 0x1F)));
  prompt_bar->SetBackground(
      views::CreateSolidBackground(kPromptBarBackgroundColor));

  // Create the explanation label.
  std::vector<size_t> offsets;
  const std::u16string learn_more_text =
      l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  const std::u16string signin_explanation_text = l10n_util::GetStringFUTF16(
      prompt_for_new_profile_
          ? IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITH_PROFILE_CREATION
          : IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITHOUT_PROFILE_CREATION,
      username, learn_more_text, &offsets);
  auto explanation_label = std::make_unique<views::StyledLabel>();
  explanation_label->SetText(signin_explanation_text);
  explanation_label->AddStyleRange(
      gfx::Range(offsets[1], offsets[1] + learn_more_text.size()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &ProfileSigninConfirmationDialogViews::LearnMoreClicked,
          base::Unretained(this))));

  // Layout the components.
  const gfx::Insets content_insets =
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::CONTROL, views::TEXT);
  // The prompt bar needs to go to the edge of the dialog, so remove horizontal
  // insets.
  SetBorder(views::CreateEmptyBorder(content_insets.top(), 0,
                                     content_insets.bottom(), 0));
  views::GridLayout* dialog_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Use GridLayout inside the prompt bar because StyledLabel requires it.
  views::GridLayout* prompt_layout =
      prompt_bar->SetLayoutManager(std::make_unique<views::GridLayout>());
  prompt_bar->SetBorder(
      views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_DIALOG_SUBSECTION)));
  constexpr int kPromptBarColumnSetId = 0;
  auto* prompt_columnset = prompt_layout->AddColumnSet(kPromptBarColumnSetId);
  prompt_columnset->AddColumn(
      views::GridLayout::FILL, views::GridLayout::CENTER,
      views::GridLayout::kFixedSize,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  prompt_columnset->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING));
  prompt_columnset->AddColumn(
      views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  prompt_layout->StartRow(views::GridLayout::kFixedSize, kPromptBarColumnSetId);
  prompt_layout->AddView(std::move(business_icon));
  prompt_layout->AddView(std::move(prompt_label));

  // Use a column set with no padding.
  dialog_layout->AddColumnSet(0)->AddColumn(
      views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  dialog_layout->StartRow(views::GridLayout::kFixedSize, 0);
  dialog_layout->AddView(std::move(prompt_bar), 1, 1, views::GridLayout::FILL,
                         views::GridLayout::FILL, 0, 0);

  // Use a new column set for the explanation label so we can add padding.
  dialog_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                               content_insets.top());
  constexpr int kExplanationColumnSetId = 1;
  views::ColumnSet* explanation_columns =
      dialog_layout->AddColumnSet(kExplanationColumnSetId);
  explanation_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        content_insets.left());
  explanation_columns->AddColumn(
      views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  explanation_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        content_insets.right());
  dialog_layout->StartRow(views::GridLayout::kFixedSize,
                          kExplanationColumnSetId);
  const int kPreferredWidth = 440;
  int explanation_label_height =
      explanation_label->GetHeightForWidth(kPreferredWidth);
  dialog_layout->AddView(std::move(explanation_label), 1, 1,
                         views::GridLayout::FILL, views::GridLayout::FILL,
                         kPreferredWidth, explanation_label_height);
}

void ProfileSigninConfirmationDialogViews::BuildWorkProfileView() {
  DCHECK(use_work_profile_wording_);

  // Create the explanation label first row.
  auto explanation_first_row = std::make_unique<views::Label>();
  explanation_first_row->SetText(
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_SIGNIN_WORK_PROFILE_CREATION));
  explanation_first_row->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  explanation_first_row->SetMultiLine(true);

  auto explanation_second_row = std::make_unique<views::Label>();
  explanation_second_row->SetText(l10n_util::GetStringUTF16(
      IDS_ENTERPRISE_SIGNIN_WORK_PROFILE_ISOLATION_NOTICE));
  explanation_second_row->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);

  // Create the explanation label.
  size_t learn_more_offset;
  const std::u16string learn_more_text =
      l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  const std::u16string signin_explanation_text =
      l10n_util::GetStringFUTF16(IDS_ENTERPRISE_SIGNIN_WORK_PROFILE_EXPLANATION,
                                 learn_more_text, &learn_more_offset);
  auto explanation_label = std::make_unique<views::StyledLabel>();
  explanation_label->SetText(signin_explanation_text);
  explanation_label->AddStyleRange(
      gfx::Range(learn_more_offset, learn_more_offset + learn_more_text.size()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &ProfileSigninConfirmationDialogViews::LearnMoreClicked,
          base::Unretained(this))));

  // Layout the components.
  const gfx::Insets content_insets =
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::CONTROL, views::TEXT);
  // The prompt bar needs to go to the edge of the dialog, so remove horizontal
  // insets.
  SetBorder(views::CreateEmptyBorder(content_insets.top(), 0,
                                     content_insets.bottom(), 0));
  views::GridLayout* dialog_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Use a new column set for the explanation label so we can add padding.
  dialog_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                               content_insets.top());
  constexpr int kExplanationColumnSetId = 1;
  views::ColumnSet* explanation_columns =
      dialog_layout->AddColumnSet(kExplanationColumnSetId);
  explanation_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        content_insets.left());
  explanation_columns->AddColumn(
      views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  explanation_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        content_insets.right());
  dialog_layout->StartRow(views::GridLayout::kFixedSize,
                          kExplanationColumnSetId);
  const int kPreferredWidth = 440;
  int explanation_first_row_height =
      explanation_first_row->GetHeightForWidth(kPreferredWidth);
  int explanation_second_row_height =
      explanation_second_row->GetHeightForWidth(kPreferredWidth);
  int explanation_label_height =
      explanation_label->GetHeightForWidth(kPreferredWidth);
  dialog_layout->AddView(std::move(explanation_first_row), 1, 1,
                         views::GridLayout::FILL, views::GridLayout::FILL,
                         kPreferredWidth, explanation_first_row_height);
  dialog_layout->StartRowWithPadding(views::GridLayout::kFixedSize,
                                     kExplanationColumnSetId,
                                     views::GridLayout::kFixedSize, 10);
  dialog_layout->AddView(std::move(explanation_second_row), 1, 1,
                         views::GridLayout::FILL, views::GridLayout::FILL,
                         kPreferredWidth, explanation_second_row_height);
  dialog_layout->StartRowWithPadding(views::GridLayout::kFixedSize,
                                     kExplanationColumnSetId,
                                     views::GridLayout::kFixedSize, 10);
  dialog_layout->AddView(std::move(explanation_label), 1, 1,
                         views::GridLayout::FILL, views::GridLayout::FILL,
                         kPreferredWidth, explanation_label_height);
}

void ProfileSigninConfirmationDialogViews::ContinueSigninButtonPressed() {
  DCHECK(prompt_for_new_profile_);
  if (delegate_) {
    delegate_->OnContinueSignin();
    delegate_ = nullptr;
  }
  GetWidget()->Close();
}

void ProfileSigninConfirmationDialogViews::LearnMoreClicked(
    const ui::Event& event) {
  NavigateParams params(
      browser_, GURL("https://support.google.com/chromebook/answer/1331549"),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = ui::DispositionFromEventFlags(
      event.flags(), WindowOpenDisposition::NEW_POPUP);
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);
}

BEGIN_METADATA(ProfileSigninConfirmationDialogViews, views::DialogDelegateView)
END_METADATA
