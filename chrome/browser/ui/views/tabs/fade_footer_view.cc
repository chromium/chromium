// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fade_footer_view.h"

#include "base/check.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/performance_manager/public/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/views/border.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"

namespace {
constexpr int kIconLabelSpacing = 8;
constexpr int kFooterVerticalMargins = 8;
constexpr int kFooterHorizontalMargins = 12;
constexpr auto kFooterBorderThickness = gfx::Insets::TLBR(1, 0, 0, 0);
constexpr auto kFooterMargins =
    gfx::Insets::VH(kFooterVerticalMargins, kFooterHorizontalMargins);
constexpr auto kFooterRefreshMargins = gfx::Insets::VH(12, 12);
}  // namespace

template <typename T>
FooterRow<T>::FooterRow(bool is_fade_out_view)
    : is_fade_out_view_(is_fade_out_view) {
  views::FlexLayout* flex_layout =
      views::View::SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  icon_ = views::View::AddChildView(std::make_unique<views::ImageView>());

  if (is_fade_out_view) {
    icon_->SetPaintToLayer();
    icon_->layer()->SetOpacity(0.0f);
  }

  icon_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  footer_label_ = views::View::AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT));
  icon_->SetBackground(
      views::CreateThemedSolidBackground(ui::kColorBubbleFooterBackground));
  footer_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  footer_label_->SetMultiLine(true);
  footer_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded, true));

  if (features::IsChromeRefresh2023()) {
    footer_label_->SetEnabledColorId(kColorTabHoverCardSecondaryText);
    footer_label_->SetTextStyle(views::style::STYLE_BODY_4);
  }

  // Vertically align the icon to the top line of the label
  const int offset = (footer_label_->GetLineHeight() -
                      GetLayoutConstant(TAB_ALERT_INDICATOR_ICON_WIDTH)) /
                     2;
  icon_->SetProperty(views::kMarginsKey,
                     gfx::Insets::TLBR(offset, 0, 0, kIconLabelSpacing));
}

template <typename T>
void FooterRow<T>::SetContent(const ui::ImageModel& icon_image_model,
                              std::u16string label_text,
                              int max_footer_width) {
  footer_label_->SetText(label_text);
  footer_label_->SetVisible(!label_text.empty());
  icon_->SetImage(icon_image_model);

  // Need to set maximum width for the label so that enough space is allocated
  // for the label to wrap properly
  const int max_label_width =
      max_footer_width - (2 * kFooterHorizontalMargins) -
      icon_->CalculatePreferredSize().width() - kIconLabelSpacing;
  footer_label_->SizeToFit(max_label_width);
  views::View::InvalidateLayout();
}

template <typename T>
gfx::Size FooterRow<T>::CalculatePreferredSize() const {
  if (footer_label_->GetText().empty()) {
    return gfx::Size();
  }

  const gfx::Size label_size = footer_label_->GetPreferredSize();
  const int width = icon_->GetPreferredSize().width() + label_size.width() +
                    kIconLabelSpacing;
  return gfx::Size(width, label_size.height());
}

template <typename T>
void FooterRow<T>::SetFade(double percent) {
  CHECK(is_fade_out_view_);
  percent = std::min(1.0, percent);
  icon_->layer()->SetOpacity(1.0 - percent);
  const SkAlpha alpha = base::saturated_cast<SkAlpha>(
      std::numeric_limits<SkAlpha>::max() * (1.0 - percent));
  footer_label_->SetBackgroundColor(
      SkColorSetA(footer_label_->GetBackgroundColor(), alpha));
  footer_label_->SetEnabledColor(
      SkColorSetA(footer_label_->GetEnabledColor(), alpha));
}

using FooterRow_AlertFooterRowData = FooterRow<AlertFooterRowData>;
BEGIN_TEMPLATE_METADATA(FooterRow_AlertFooterRowData, FooterRow)
END_METADATA

using FooterRow_PerformanceRowData = FooterRow<PerformanceRowData>;
BEGIN_TEMPLATE_METADATA(FooterRow_PerformanceRowData, FooterRow)
END_METADATA

template class FooterRow<AlertFooterRowData>;
template class FooterRow<PerformanceRowData>;

// FadeAlertFooterRow
// -----------------------------------------------------------------------

void FadeAlertFooterRow::SetData(const AlertFooterRowData& data) {
  absl::optional<TabAlertState> alert_state = data.alert_state;
  if (alert_state.has_value()) {
    SetContent(AlertIndicatorButton::GetTabAlertIndicatorImageForHoverCard(
                   alert_state.value()),
               chrome::GetTabAlertStateText(alert_state.value()),
               data.footer_row_width);
  } else {
    SetContent(ui::ImageModel(), std::u16string(), data.footer_row_width);
  }
  data_ = data;
}

BEGIN_METADATA(FadeAlertFooterRow)
END_METADATA

// FadePerformanceFooterRow
// -----------------------------------------------------------------------

void FadePerformanceFooterRow::SetData(const PerformanceRowData& data) {
  std::u16string row_text;
  if (data.should_show_discard_status) {
    if (data.memory_savings_in_bytes > 0) {
      const std::u16string formatted_memory_usage =
          ui::FormatBytes(data.memory_savings_in_bytes);
      row_text = l10n_util::GetStringFUTF16(
          IDS_HOVERCARD_INACTIVE_TAB_MEMORY_SAVINGS, formatted_memory_usage);
    } else {
      row_text = l10n_util::GetStringUTF16(IDS_HOVERCARD_INACTIVE_TAB);
    }
  } else if (data.memory_usage_in_bytes > 0) {
    const std::u16string formatted_memory_usage =
        ui::FormatBytes(data.memory_usage_in_bytes);
    row_text = l10n_util::GetStringFUTF16(
        data.memory_usage_in_bytes >
                static_cast<uint64_t>(
                    performance_manager::features::
                        kMemoryUsageInHovercardsHighThresholdBytes.Get())
            ? IDS_HOVERCARD_TAB_HIGH_MEMORY_USAGE
            : IDS_HOVERCARD_TAB_MEMORY_USAGE,
        formatted_memory_usage);
  }

  const ui::ImageModel icon_image_model =
      row_text.empty()
          ? ui::ImageModel()
          : ui::ImageModel::FromVectorIcon(
                kHighEfficiencyIcon, kColorHoverCardTabAlertAudioPlayingIcon,
                GetLayoutConstant(TAB_ALERT_INDICATOR_ICON_WIDTH));

  SetContent(icon_image_model, row_text, data.footer_row_width);
  data_ = data;
}

BEGIN_METADATA(FadePerformanceFooterRow)
END_METADATA

// FooterView
// -----------------------------------------------------------------------

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(FooterView, kHoverCardFooterElementId);

FooterView::FooterView() {
  SetProperty(views::kElementIdentifierKey, kHoverCardFooterElementId);
  const gfx::Insets footer_margins =
      features::IsChromeRefresh2023() ? kFooterRefreshMargins : kFooterMargins;
  flex_layout_ =
      views::View::SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout_->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true)
      .SetInteriorMargin(footer_margins)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::VH(kFooterVerticalMargins, 0));
  alert_row_ = AddChildView(std::make_unique<AlertFadeView>(
      std::make_unique<FadeAlertFooterRow>(/* is_fade_out_view =*/false),
      std::make_unique<FadeAlertFooterRow>(/* is_fade_out_view =*/true)));

  performance_row_ = AddChildView(std::make_unique<PerformanceFadeView>(
      std::make_unique<FadePerformanceFooterRow>(/* is_fade_out_view =*/false),
      std::make_unique<FadePerformanceFooterRow>(/* is_fade_out_view =*/true)));

  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorBubbleFooterBackground));

  if (!features::IsChromeRefresh2023()) {
    SetBorder(views::CreateThemedSolidSidedBorder(
        kFooterBorderThickness, ui::kColorBubbleFooterBorder));
  }
}

void FooterView::SetAlertData(const AlertFooterRowData& data) {
  alert_row_->SetData(data);
  UpdateVisibility();
}

void FooterView::SetPerformanceData(const PerformanceRowData& data) {
  performance_row_->SetData(data);
  UpdateVisibility();
}

void FooterView::SetFade(double percent) {
  alert_row_->SetFade(percent);
  performance_row_->SetFade(percent);
}

void FooterView::UpdateVisibility() {
  SetVisible(performance_row_->CalculatePreferredSize().height() > 0 ||
             alert_row_->CalculatePreferredSize().height() > 0);
}

gfx::Size FooterView::GetMinimumSize() const {
  return gfx::Size();
}

using FadeWrapper_View_PerformanceRowData =
    FadeWrapper<views::View, PerformanceRowData>;

BEGIN_TEMPLATE_METADATA(FadeWrapper_View_PerformanceRowData, FadeWrapper)
END_METADATA

using FadeWrapper_View_AlertFooterRowData =
    FadeWrapper<views::View, AlertFooterRowData>;

BEGIN_TEMPLATE_METADATA(FadeWrapper_View_AlertFooterRowData, FadeWrapper)
END_METADATA

using FadeView_FadeAlertFooterRow_FadeAlertFooterRow_AlertFooterRowData =
    FadeView<FadeAlertFooterRow, FadeAlertFooterRow, AlertFooterRowData>;

BEGIN_TEMPLATE_METADATA(
    FadeView_FadeAlertFooterRow_FadeAlertFooterRow_AlertFooterRowData,
    FadeView)
END_METADATA

using FadeView_FadePerformanceFooterRow_FadePerformanceFooterRow_PerformanceRowData =
    FadeView<FadePerformanceFooterRow,
             FadePerformanceFooterRow,
             PerformanceRowData>;

BEGIN_TEMPLATE_METADATA(
    FadeView_FadePerformanceFooterRow_FadePerformanceFooterRow_PerformanceRowData,
    FadeView)
END_METADATA

BEGIN_METADATA(FooterView, views::View)
END_METADATA
