// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/save_to_drive/account_chooser_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_radio_group_view.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace save_to_drive {
AccountChooserView::AccountChooserView(
    AccountChooserControllerDelegate* account_chooser_controller_delegate,
    AccountChooserViewDelegate* parent_dialog,
    const std::vector<AccountInfo>& accounts,
    std::optional<CoreAccountId> primary_account_id)
    : account_chooser_controller_delegate_(account_chooser_controller_delegate),
      parent_dialog_(parent_dialog) {
  SetOrientation(views::LayoutOrientation::kVertical);
  header_view_ = AddChildView(CreateHeaderView(accounts));
  body_view_ = AddChildView(CreateBodyView(accounts, primary_account_id));
  footer_view_ = AddChildView(CreateFooterView());
}
AccountChooserView::~AccountChooserView() = default;

void AccountChooserView::UpdateView(
    const std::vector<AccountInfo>& accounts,
    std::optional<CoreAccountId> primary_account_id) {
  UpdateHeaderView(accounts);
  UpdateBodyView(accounts, primary_account_id);
}

std::unique_ptr<views::View> AccountChooserView::CreateBodyMultiAccount(
    const std::vector<AccountInfo>& accounts,
    std::optional<CoreAccountId> primary_account_id) {
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  views::View* const content =
      scroll_view->SetContents(std::make_unique<AccountChooserRadioGroupView>(
          *parent_dialog_, accounts, primary_account_id));

  int per_account_size = content->GetPreferredSize().height() / accounts.size();
  scroll_view->ClipHeightTo(
      0, static_cast<int>(per_account_size * kMaxAccountsToShow));
  return scroll_view;
}

std::unique_ptr<views::View> AccountChooserView::CreateBodySingleAccount(
    const AccountInfo& account) {
  return views::Builder<views::FlexLayoutView>()
      .SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded))
      .SetOrientation(views::LayoutOrientation::kVertical)
      .AddChildren(
          views::Builder<views::Separator>(),
          views::Builder<views::FlexLayoutView>()
              .SetOrientation(views::LayoutOrientation::kVertical)
              .SetInteriorMargin(gfx::Insets::VH(
                  /*vertical=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                      DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN),
                  /*horizontal=*/0))
              .AddChildren(
                  views::Builder<views::View>(CreateAccountRow(account))),
          views::Builder<views::Separator>())
      .Build();
}

std::unique_ptr<views::View> AccountChooserView::CreateBodyView(
    const std::vector<AccountInfo>& accounts,
    std::optional<CoreAccountId> primary_account_id) {
  CHECK(IsSingleAccount(accounts) || IsMultiAccount(accounts))
      << "Account chooser view should "
         "only be used if there are one or more accounts.";
  if (IsSingleAccount(accounts)) {
    parent_dialog_->OnAccountSelected(accounts.front());
    return CreateBodySingleAccount(accounts.front());
  } else {
    return CreateBodyMultiAccount(accounts, primary_account_id);
  }
}

std::unique_ptr<views::View> AccountChooserView::CreateDriveLogoView() {
  auto drive_logo_view = std::make_unique<views::BoxLayoutView>();
  drive_logo_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  drive_logo_view->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_EXTENSIONS_MENU_LABEL_ICON_SPACING));
  drive_logo_view->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          omnibox::kDriveLogoIcon, ui::kColorIcon,
          ChromeLayoutProvider::Get()->GetDistanceMetric(
              DISTANCE_TOAST_BUBBLE_ICON_SIZE))));
  drive_logo_view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ACCOUNT_CHOOSER_DRIVE),
      views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_BODY_3_MEDIUM));
  return drive_logo_view;
}

std::unique_ptr<views::View> AccountChooserView::CreateFooterView() {
  const views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  auto footer = std::make_unique<views::FlexLayoutView>();
  footer->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
  footer->SetIgnoreDefaultMainAxisMargins(true);
  footer->SetDefault(views::kMarginsKey,
                     gfx::Insets::TLBR(
                         /*top=*/0, /*left=*/
                         layout_provider->GetDistanceMetric(
                             views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                         /*bottom=*/0, /*right=*/0));
  footer->SetInteriorMargin(gfx::Insets::TLBR(
      /*top=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW),
      /*left=*/0,
      /*bottom=*/0,
      /*right=*/0));

  // Add the "Use a different account" button.
  auto add_account_button_container = std::make_unique<views::FlexLayoutView>();
  auto use_other_account_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          &AccountChooserControllerDelegate::ShowAddAccountDialog,
          base::Unretained(account_chooser_controller_delegate_)),
      l10n_util::GetStringUTF16(IDS_ACCOUNT_CHOOSER_ADD_ACCOUNT));
  use_other_account_button->SetStyle(ui::ButtonStyle::kDefault);
  use_other_account_button->SetAppearDisabledInInactiveWidget(true);
  add_account_button_container->AddChildView(
      std::move(use_other_account_button));
  // Ensure the button is left-aligned.
  add_account_button_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  footer->AddChildView(std::move(add_account_button_container));

  // Add the "Cancel" button.
  auto cancel_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          &AccountChooserViewDelegate::OnUserClosedDialog,
          base::Unretained(parent_dialog_),
          static_cast<int32_t>(
              views::Widget::ClosedReason::kCancelButtonClicked)),
      l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button->SetStyle(ui::ButtonStyle::kTonal);
  cancel_button->SetAppearDisabledInInactiveWidget(true);
  footer->AddChildView(std::move(cancel_button));

  // Add the "Save" button.
  auto save_button = std::make_unique<views::MdTextButton>(
      // Save button eventually calls a OnceCallback
      base::BindOnce(&AccountChooserViewDelegate::OnSaveButtonClicked,
                     base::Unretained(parent_dialog_)),
      l10n_util::GetStringUTF16(IDS_SAVE));
  save_button->SetStyle(ui::ButtonStyle::kProminent);
  save_button->SetAppearDisabledInInactiveWidget(true);
  footer->AddChildView(std::move(save_button));

  return footer;
}

std::unique_ptr<views::View> AccountChooserView::CreateHeaderView(
    const std::vector<AccountInfo>& accounts) {
  auto header = std::make_unique<views::BoxLayoutView>();
  header->SetOrientation(views::BoxLayout::Orientation::kVertical);
  header->SetInsideBorderInsets(
      gfx::Insets::TLBR(0, 0,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE),
                        0));
  header->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  views::FlexSpecification flex_spec(views::LayoutOrientation::kHorizontal,
                                     views::MinimumFlexSizeRule::kScaleToZero,
                                     views::MaximumFlexSizeRule::kUnbounded);
  header->SetProperty(views::kFlexBehaviorKey, flex_spec);

  // Add the title.
  header->AddChildView(CreateTitleView(accounts));

  // Add the subtitle.
  header->AddChildView(CreateSubtitleLabel());

  return header;
}

std::unique_ptr<views::Label> AccountChooserView::CreateTitleLabel(
    const std::vector<AccountInfo>& accounts) {
  auto title_label = std::make_unique<views::Label>(
      GetTitle(accounts), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_HEADLINE_4_BOLD);
  SetLabelProperties(title_label.get());
  return title_label;
}

std::unique_ptr<views::StyledLabel> AccountChooserView::CreateSubtitleLabel() {
  auto subtitle_label = std::make_unique<views::StyledLabel>();

  subtitle_label->SetDefaultTextStyle(views::style::STYLE_BODY_3);
  subtitle_label->SetTextContext(views::style::CONTEXT_DIALOG_TITLE);

  std::u16string saved_from_chrome =
      l10n_util::GetStringUTF16(IDS_ACCOUNT_CHOOSER_SAVED_FROM_CHROME);

  // Find the offsets of the text to style.
  std::vector<size_t> offsets;
  std::u16string text = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(IDS_ACCOUNT_CHOOSER_SUBTITLE),
      {saved_from_chrome}, &offsets);
  subtitle_label->SetText(text);
  subtitle_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  gfx::Range saved_from_chrome_range(offsets[0],
                                     offsets[0] + saved_from_chrome.length());
  views::StyledLabel::RangeStyleInfo style_info;
  style_info.text_style = views::style::STYLE_BODY_3_MEDIUM;
  subtitle_label->AddStyleRange(saved_from_chrome_range, style_info);
  return subtitle_label;
}

std::unique_ptr<views::View> AccountChooserView::CreateTitleView(
    const std::vector<AccountInfo>& accounts) {
  auto title_view = std::make_unique<views::FlexLayoutView>();
  title_view->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  auto title_container = std::make_unique<views::FlexLayoutView>();
  title_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  title_view->AddChildView(CreateTitleLabel(accounts));
  title_view->AddChildView(CreateDriveLogoView());
  return title_view;
}

std::u16string AccountChooserView::GetTitle(
    const std::vector<AccountInfo>& accounts) {
  if (IsSingleAccount(accounts)) {
    return l10n_util::GetStringUTF16(IDS_ACCOUNT_CHOOSER_SINGLE_ACCOUNT_TITLE);
  } else {
    return l10n_util::GetStringUTF16(IDS_ACCOUNT_CHOOSER_MULTI_ACCOUNT_TITLE);
  }
}

bool AccountChooserView::IsMultiAccount(
    const std::vector<AccountInfo>& accounts) {
  return accounts.size() > 1;
}

bool AccountChooserView::IsSingleAccount(
    const std::vector<AccountInfo>& accounts) {
  return accounts.size() == 1;
}

void AccountChooserView::SetLabelProperties(views::Label* label) {
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
}

void AccountChooserView::UpdateBodyView(
    const std::vector<AccountInfo>& accounts,
    std::optional<CoreAccountId> primary_account_id) {
  std::optional<size_t> index = GetIndexOf(body_view_);
  CHECK(index.has_value());
  RemoveChildViewT(body_view_.ExtractAsDangling());
  body_view_ = AddChildViewAt(CreateBodyView(accounts, primary_account_id),
                              index.value());
}

void AccountChooserView::UpdateHeaderView(
    const std::vector<AccountInfo>& accounts) {
  std::optional<size_t> index = GetIndexOf(header_view_);
  CHECK(index.has_value());
  RemoveChildViewT(header_view_.ExtractAsDangling());
  header_view_ = AddChildViewAt(CreateHeaderView(accounts), index.value());
}

BEGIN_METADATA(AccountChooserView)
END_METADATA
}  // namespace save_to_drive
