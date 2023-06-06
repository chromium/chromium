// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fade_footer_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/performance_manager/public/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"

template <typename T>
FooterRow<T>::FooterRow() {
  views::FlexLayout* flex_layout =
      views::View::SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(views::kMarginsKey,
                  views::LayoutProvider::Get()->GetInsetsMetric(
                      views::InsetsMetric::INSETS_ICON_BUTTON));

  icon_ = views::View::AddChildView(std::make_unique<views::ImageView>());
  icon_->SetPaintToLayer();
  icon_->layer()->SetOpacity(0.0f);
  footer_label_ = views::View::AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT));
  footer_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  footer_label_->SetMultiLine(true);
  footer_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded, true));
}

template <typename T>
gfx::Size FooterRow<T>::CalculatePreferredSize() const {
  if (footer_label_->GetText().empty()) {
    return gfx::Size();
  }

  const gfx::Size label_size = footer_label_->GetPreferredSize();
  const int width = icon_->GetPreferredSize().width() + label_size.width();
  const int height = label_size.height();
  return gfx::Size(width, height);
}

template <typename T>
void FooterRow<T>::SetFade(double percent) {
  percent = std::min(1.0, percent);
  icon_->layer()->SetOpacity(1.0 - percent);
  const SkAlpha alpha = base::saturated_cast<SkAlpha>(
      std::numeric_limits<SkAlpha>::max() * (1.0 - percent));
  footer_label_->SetBackgroundColor(
      SkColorSetA(footer_label_->GetBackgroundColor(), alpha));
  footer_label_->SetEnabledColor(
      SkColorSetA(footer_label_->GetEnabledColor(), alpha));
}

template <typename T>
void FooterRow<T>::UpdateIconAndLabelLayout(int max_footer_width) {
  icon_->layer()->SetOpacity(1.0f);
  auto* const color_provider = views::View::GetColorProvider();
  icon_->SetBackground(views::CreateSolidBackground(
      color_provider->GetColor(ui::kColorBubbleFooterBackground)));

  // Need to set maximum width for the label so that enough space is allocated
  // for the label to wrap properly
  const int max_label_width = max_footer_width -
                              (2 * kFooterHorizontalMargins) -
                              icon_->CalculatePreferredSize().width();
  footer_label_->SizeToFit(max_label_width);

  // Calculate difference between label height and icon size to vertically
  // align the icon with the label
  const int label_height = footer_label_->GetPreferredSize().height();
  const int icon_height = icon_->GetPreferredSize().height();
  const int margin = (label_height - icon_height) / 2;
  icon_->SetProperty(views::kMarginsKey, gfx::Insets::VH(margin, 0));
  views::View::InvalidateLayout();
}

template class FooterRow<AlertFooterRowData>;
template class FooterRow<PerformanceRowData>;

// FadeAlertFooterRow
// -----------------------------------------------------------------------

void FadeAlertFooterRow::SetData(const AlertFooterRowData& data) {
  absl::optional<TabAlertState> alert_state = data.alert_state;
  views::Label* const alert_label = footer_label();
  views::ImageView* const alert_icon = icon();
  if (alert_state.has_value()) {
    CHECK(data.icon_color.has_value());
    alert_label->SetText(chrome::GetTabAlertStateText(alert_state.value()));
    alert_icon->SetImage(AlertIndicatorButton::GetTabAlertIndicatorImage(
        alert_state.value(), data.icon_color.value()));
    UpdateIconAndLabelLayout(data.footer_row_width);
  } else {
    alert_label->SetText(std::u16string());
    alert_icon->SetImage(ui::ImageModel());
    alert_icon->layer()->SetOpacity(0.0f);
  }
  data_ = data;
}

// FadePerformanceFooterRow
// -----------------------------------------------------------------------

void FadePerformanceFooterRow::SetData(const PerformanceRowData& data) {
  views::Label* const performance_label = footer_label();
  views::ImageView* const performance_icon = icon();
  if (data.should_show_discard_status) {
    if (data.memory_savings_in_bytes > 0) {
      std::u16string formatted_memory_usage =
          ui::FormatBytes(data.memory_savings_in_bytes);
      std::u16string memory_usage_with_placeholder =
          l10n_util::GetStringUTF16(IDS_HOVERCARD_INACTIVE_TAB_MEMORY_SAVINGS);
      performance_label->SetText(l10n_util::FormatString(
          memory_usage_with_placeholder, {formatted_memory_usage}, nullptr));
    } else {
      performance_label->SetText(
          l10n_util::GetStringUTF16(IDS_HOVERCARD_INACTIVE_TAB));
    }
    performance_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kHighEfficiencyIcon, ui::kColorMenuIcon, gfx::kFaviconSize));
    UpdateIconAndLabelLayout(data.footer_row_width);
  } else if (data.memory_usage_in_bytes > 0) {
    const std::u16string formatted_memory_usage =
        ui::FormatBytes(data.memory_usage_in_bytes);
    const std::u16string memory_usage_with_placeholder =
        data.memory_usage_in_bytes >
                static_cast<uint64_t>(
                    performance_manager::features::
                        kMemoryUsageInHovercardsHighThresholdBytes.Get())
            ? l10n_util::GetStringUTF16(IDS_HOVERCARD_TAB_HIGH_MEMORY_USAGE)
            : l10n_util::GetStringUTF16(IDS_HOVERCARD_TAB_MEMORY_USAGE);
    performance_label->SetText(l10n_util::FormatString(
        memory_usage_with_placeholder, {formatted_memory_usage}, nullptr));
    performance_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kHighEfficiencyIcon, ui::kColorMenuIcon, gfx::kFaviconSize));
    UpdateIconAndLabelLayout(data.footer_row_width);
  } else {
    performance_label->SetText(std::u16string());
    performance_icon->SetImage(ui::ImageModel());
    performance_icon->layer()->SetOpacity(0.0f);
  }
  data_ = data;
}

// FooterView
// -----------------------------------------------------------------------

void FooterView::OnThemeChanged() {
  views::View::OnThemeChanged();
  auto* const color_provider = GetColorProvider();
  SetBackground(views::CreateSolidBackground(
      color_provider->GetColor(ui::kColorBubbleFooterBackground)));
  views::View::SetBorder(views::CreateSolidSidedBorder(
      is_using_alternate_layout_ ? gfx::Insets::TLBR(0, 0, 1, 0)
                                 : gfx::Insets::TLBR(1, 0, 0, 0),
      color_provider->GetColor(ui::kColorBubbleFooterBorder)));
}

gfx::Size FooterView::CalculatePreferredSize() const {
  gfx::Size preferred_size = alert_row_->CalculatePreferredSize();
  preferred_size += performance_row_->CalculatePreferredSize();

  // Add additional margin space when the footer have content to show
  if (preferred_size.width() > 0 && preferred_size.height() > 0) {
    const gfx::Insets margins = flex_layout_->interior_margin();
    preferred_size.Enlarge(margins.width(), margins.height());
  }
  return preferred_size;
}
