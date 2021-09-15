// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/security_information_view.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/page_info/features.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

SecurityInformationView::SecurityInformationView(int side_margin) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int icon_label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  const int label_column_status = 1;
  views::ColumnSet* column_set = layout->AddColumnSet(label_column_status);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, side_margin);

  if (base::FeatureList::IsEnabled(page_info::kPageInfoV2Desktop)) {
    // Page info v2 has icon on the left side and all other content is indented
    // by the icon size.
    column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                          views::GridLayout::kFixedSize,
                          views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
    column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                 icon_label_spacing);
  }
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, side_margin);

  if (base::FeatureList::IsEnabled(page_info::kPageInfoV2Desktop)) {
    // Add padding before the title so that it's in the same position as when
    // this control is a hover button.
    auto hover_button_insets = layout_provider->GetInsetsMetric(
        ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);
    layout->AddPaddingRow(views::GridLayout::kFixedSize,
                          hover_button_insets.top());
    layout->StartRow(views::GridLayout::kFixedSize, label_column_status);
    icon_ = layout->AddView(std::make_unique<NonAccessibleImageView>());

    auto security_summary_label = std::make_unique<views::StyledLabel>();
    // TODO(olesiamarukhno): Check padding between summary and description
    // labels after more UI is implemented.
    security_summary_label->SetTextContext(
        views::style::CONTEXT_DIALOG_BODY_TEXT);
    security_summary_label->SetID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_SUMMARY_LABEL);
    security_summary_label_ =
        layout->AddView(std::move(security_summary_label), 1.0, 1.0,
                        views::GridLayout::FILL, views::GridLayout::LEADING);
  }

  auto start_secondary_row = [=]() {
    layout->StartRow(views::GridLayout::kFixedSize, label_column_status);
    // Skipping the icon's column.
    if (base::FeatureList::IsEnabled(page_info::kPageInfoV2Desktop))
      layout->SkipColumns(1);
  };

  start_secondary_row();
  auto security_details_label = std::make_unique<views::StyledLabel>();
  security_details_label->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_DETAILS_LABEL);
  security_details_label_ =
      layout->AddView(std::move(security_details_label), 1.0, 1.0,
                      views::GridLayout::FILL, views::GridLayout::LEADING);
  if (base::FeatureList::IsEnabled(page_info::kPageInfoV2Desktop))
    security_details_label_->SetDefaultTextStyle(views::style::STYLE_SECONDARY);

  start_secondary_row();
  auto reset_decisions_label_container = std::make_unique<views::View>();
  reset_decisions_label_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  reset_decisions_label_container_ =
      layout->AddView(std::move(reset_decisions_label_container), 1.0, 1.0,
                      views::GridLayout::FILL, views::GridLayout::LEADING);

  start_secondary_row();
  password_reuse_button_container_ =
      layout->AddView(std::make_unique<views::View>(), 1, 1,
                      views::GridLayout::FILL, views::GridLayout::LEADING);

  if (base::FeatureList::IsEnabled(page_info::kPageInfoV2Desktop)) {
    const int end_padding =
        layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL);
    layout->AddPaddingRow(views::GridLayout::kFixedSize, end_padding);
  }
}

SecurityInformationView::~SecurityInformationView() = default;

void SecurityInformationView::SetIcon(const ui::ImageModel& image_icon) {
  icon_->SetImage(image_icon);
}

void SecurityInformationView::SetSummary(const std::u16string& summary_text,
                                         int text_style) {
  security_summary_label_->SetText(summary_text);
  security_summary_label_->SetDefaultTextStyle(text_style);
}

void SecurityInformationView::SetDetails(
    const std::u16string& details_text,
    views::Link::ClickedCallback security_details_callback) {
  std::vector<std::u16string> subst;
  subst.push_back(details_text);
  subst.push_back(l10n_util::GetStringUTF16(IDS_LEARN_MORE));

  std::vector<size_t> offsets;

  std::u16string text =
      base::ReplaceStringPlaceholders(u"$1 $2", subst, &offsets);
  security_details_label_->SetText(text);
  gfx::Range details_range(offsets[1], text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          security_details_callback);

  security_details_label_->AddStyleRange(details_range, link_style);
}

void SecurityInformationView::AddResetDecisionsLabel(
    base::RepeatingClosure reset_decisions_callback) {
  if (!reset_decisions_label_container_->children().empty()) {
    // Ensure all old content is removed from the container before re-adding it.
    reset_decisions_label_container_->RemoveAllChildViews();
  }

  std::vector<std::u16string> subst;
  subst.push_back(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_INVALID_CERTIFICATE_DESCRIPTION));
  subst.push_back(l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_RESET_INVALID_CERTIFICATE_DECISIONS_BUTTON));

  std::vector<size_t> offsets;

  std::u16string text =
      base::ReplaceStringPlaceholders(u"$1 $2", subst, &offsets);
  views::StyledLabel* reset_cert_decisions_label =
      reset_decisions_label_container_->AddChildView(
          std::make_unique<views::StyledLabel>());
  if (base::FeatureList::IsEnabled(page_info::kPageInfoV2Desktop)) {
    reset_cert_decisions_label->SetDefaultTextStyle(
        views::style::STYLE_SECONDARY);
  }
  reset_cert_decisions_label->SetText(text);
  gfx::Range link_range(offsets[1], text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          reset_decisions_callback);
  link_style.disable_line_wrapping = false;

  reset_cert_decisions_label->AddStyleRange(link_range, link_style);
  // Fit the styled label to occupy available width.
  reset_cert_decisions_label->SizeToFit(0);

  // Now that it contains a label, the container needs padding at the top.
  const int between_paragraphs_distance =
      base::FeatureList::IsEnabled(page_info::kPageInfoV2Desktop)
          ? ChromeLayoutProvider::Get()->GetDistanceMetric(
                views::DISTANCE_RELATED_CONTROL_VERTICAL)
          : 8;
  reset_decisions_label_container_->SetBorder(views::CreateEmptyBorder(
      between_paragraphs_distance, views::GridLayout::kFixedSize,
      views::GridLayout::kFixedSize, 0));

  InvalidateLayout();
}

void SecurityInformationView::AddPasswordReuseButtons(
    PageInfo::SafeBrowsingStatus safe_browsing_status,
    views::Button::PressedCallback change_password_callback,
    views::Button::PressedCallback password_reuse_callback) {
  if (!password_reuse_button_container_->children().empty()) {
    // Ensure all old content is removed from the container before re-adding it.
    password_reuse_button_container_->RemoveAllChildViews();
  }

  int change_password_template = 0;
  switch (safe_browsing_status) {
    case PageInfo::SafeBrowsingStatus::
        SAFE_BROWSING_STATUS_SAVED_PASSWORD_REUSE:
      change_password_template = IDS_PAGE_INFO_CHECK_PASSWORDS_BUTTON;
      break;
    case PageInfo::SafeBrowsingStatus::
        SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE:
      change_password_template = IDS_PAGE_INFO_CHANGE_PASSWORD_BUTTON;
      break;
    case PageInfo::SafeBrowsingStatus::
        SAFE_BROWSING_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case PageInfo::SafeBrowsingStatus::
        SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE:
      change_password_template = IDS_PAGE_INFO_PROTECT_ACCOUNT_BUTTON;
      break;
    default:
      NOTREACHED();
      break;
  }

  std::unique_ptr<views::MdTextButton> change_password_button;
  if (change_password_template) {
    change_password_button = std::make_unique<views::MdTextButton>(
        change_password_callback,
        l10n_util::GetStringUTF16(change_password_template));
    change_password_button->SetProminent(true);
    change_password_button->SetID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  }
  auto allowlist_password_reuse_button = std::make_unique<views::MdTextButton>(
      password_reuse_callback,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_ALLOWLIST_PASSWORD_REUSE_BUTTON));
  allowlist_password_reuse_button->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_BUTTON_ALLOWLIST_PASSWORD_REUSE);

  int kSpacingBetweenButtons = 8;
  int change_password_button_size =
      change_password_button
          ? change_password_button->CalculatePreferredSize().width()
          : 0;

  // If these two buttons cannot fit into a single line, stack them vertically.
  bool can_fit_in_one_line =
      (password_reuse_button_container_->width() - kSpacingBetweenButtons) >=
      (change_password_button_size +
       allowlist_password_reuse_button->CalculatePreferredSize().width());
  bool is_page_info_v2 =
      base::FeatureList::IsEnabled(page_info::kPageInfoV2Desktop);
  auto layout = std::make_unique<views::BoxLayout>(
      can_fit_in_one_line || is_page_info_v2
          ? views::BoxLayout::Orientation::kHorizontal
          : views::BoxLayout::Orientation::kVertical,
      gfx::Insets(), kSpacingBetweenButtons);
  // Make buttons left-aligned. For RTL languages, buttons will automatically
  // become right-aligned.
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  password_reuse_button_container_->SetLayoutManager(std::move(layout));

#if defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
  if (change_password_button) {
    password_reuse_button_container_->AddChildView(
        std::move(change_password_button));
  }
  password_reuse_button_container_->AddChildView(
      std::move(allowlist_password_reuse_button));
#else
  password_reuse_button_container_->AddChildView(
      std::move(allowlist_password_reuse_button));
  if (change_password_button) {
    password_reuse_button_container_->AddChildView(
        std::move(change_password_button));
  }
#endif

  // Add padding at the top.
  password_reuse_button_container_->SetBorder(
      views::CreateEmptyBorder(8, views::GridLayout::kFixedSize, 0, 0));

  InvalidateLayout();
}

BEGIN_METADATA(SecurityInformationView, views::View)
END_METADATA
