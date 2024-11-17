// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/security_information_view.h"

#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"

SecurityInformationView::SecurityInformationView(int side_margin) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int icon_label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  auto hover_button_insets = layout_provider->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);
  // TODO(olesiamarukhno): Unify the column width through all views in the
  // page info (RichHoverButton, PermissionSelectorRow, ChosenObjectView,
  // SecurityInformationView). Currently, it isn't same everywhere and it
  // causes label text next to icon not to be aligned by 1 or 2px.
  views::TableLayout* layout =
      SetLayoutManager(std::make_unique<views::TableLayout>());
  layout->AddPaddingColumn(views::TableLayout::kFixedSize, side_margin)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStart, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, side_margin)
      // Add padding before the title so that it's in the same position as when
      // this control is a hover button.
      .AddPaddingRow(views::TableLayout::kFixedSize, hover_button_insets.top())
      .AddRows(1, views::TableLayout::kFixedSize);

  icon_ = AddChildView(std::make_unique<NonAccessibleImageView>());

  security_summary_label_ =
      AddChildView(std::make_unique<views::StyledLabel>());
  // TODO(olesiamarukhno): Check padding between summary and description
  // labels after more UI is implemented.
  security_summary_label_->SetTextContext(
      views::style::CONTEXT_DIALOG_BODY_TEXT);
  security_summary_label_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_SUMMARY_LABEL);
  security_summary_label_->SetDefaultEnabledColorId(kColorPageInfoForeground);
  // The label defaults to a single line, which would force the dialog wider;
  // instead give it a width that's the minimum we want it to have.  Then the
  // TableLayout will stretch it back out into any additional space available.
  min_label_width_ =
      PageInfoViewFactory::kMinBubbleWidth - side_margin * 2 -
      PageInfoViewFactory::GetConnectionSecureIcon().Size().width() -
      icon_label_spacing;
  security_summary_label_->SizeToFit(min_label_width_);

  auto start_secondary_row = [=, this]() {
    layout->AddRows(1, views::TableLayout::kFixedSize);
    AddChildView(std::make_unique<views::View>());  // Skipping the icon column.
  };

  start_secondary_row();
  security_details_label_ =
      AddChildView(std::make_unique<views::StyledLabel>());
  security_details_label_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_DETAILS_LABEL);
  security_details_label_->SetDefaultTextStyle(views::style::STYLE_BODY_4);
  security_details_label_->SetDefaultEnabledColorId(
      kColorPageInfoSubtitleForeground);
  security_details_label_->SizeToFit(min_label_width_);

  start_secondary_row();
  reset_decisions_label_container_ =
      AddChildView(std::make_unique<views::View>());
  reset_decisions_label_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));

  start_secondary_row();
  password_reuse_button_container_ =
      AddChildView(std::make_unique<views::View>());

  const int end_padding =
      layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL);
  layout->AddPaddingRow(views::TableLayout::kFixedSize, end_padding);
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
  reset_cert_decisions_label->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_RESET_DECISIONS_LABEL);
  reset_cert_decisions_label->SetDefaultTextStyle(views::style::STYLE_BODY_4);
  reset_cert_decisions_label->SetDefaultEnabledColorId(
      kColorPageInfoSubtitleForeground);
  reset_cert_decisions_label->SetText(text);
  gfx::Range link_range(offsets[1], text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          reset_decisions_callback);

  reset_cert_decisions_label->AddStyleRange(link_range, link_style);
  // Adjust this label's width to the width of the label above.
  reset_cert_decisions_label->SizeToFit(security_details_label_->width());

  // Now that it contains a label, the container needs padding at the top.
  const int between_paragraphs_distance =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL);
  reset_decisions_label_container_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(between_paragraphs_distance, 0, 0, 0)));

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
  }

  std::unique_ptr<views::MdTextButton> change_password_button;
  if (change_password_template) {
    change_password_button = std::make_unique<views::MdTextButton>(
        std::move(change_password_callback),
        l10n_util::GetStringUTF16(change_password_template));
    change_password_button->SetStyle(ui::ButtonStyle::kProminent);
    change_password_button->SetID(
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  }
  auto allowlist_password_reuse_button = std::make_unique<views::MdTextButton>(
      std::move(password_reuse_callback),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_ALLOWLIST_PASSWORD_REUSE_BUTTON));
  allowlist_password_reuse_button->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_BUTTON_ALLOWLIST_PASSWORD_REUSE);

  int kSpacingBetweenButtons = 8;
  // TODO(crbug.com/40800258): Fix alignment if the buttons don't fit in one
  // row.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kSpacingBetweenButtons);
  // Make buttons left-aligned. For RTL languages, buttons will automatically
  // become right-aligned.
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  password_reuse_button_container_->SetLayoutManager(std::move(layout));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
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
      views::CreateEmptyBorder(gfx::Insets::TLBR(8, 0, 0, 0)));
  int w = password_reuse_button_container_->GetPreferredSize().width();
  if (w > min_label_width_) {
    AdjustContentWidth(w);
  }

  InvalidateLayout();
}

void SecurityInformationView::AdjustContentWidth(int w) {
  security_summary_label_->SizeToFit(w);
  security_details_label_->SizeToFit(w);
}

BEGIN_METADATA(SecurityInformationView)
END_METADATA
