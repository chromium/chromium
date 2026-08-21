// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_handler_header_view_util.h"

#include <utility>

#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/icon/icon_size.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

namespace payments {

namespace {

// Returns a Google color closest to light_mode_color or dark_mode_color based
// on whether background_color is considered dark mode, with a minimum
// contrast_ratio between the returned color and the background_color.
SkColor GetContrastingGoogleColor(SkColor light_mode_color,
                                  SkColor dark_mode_color,
                                  SkColor background_color,
                                  float contrast_ratio) {
  const SkColor preferred_color = color_utils::IsDark(background_color)
                                      ? dark_mode_color
                                      : light_mode_color;
  return color_utils::PickGoogleColor(preferred_color, background_color,
                                      contrast_ratio);
}

// Computes the effective background color for header subviews (e.g. origin
// label, progress bar, close button). If `theme_color` is provided (e.g. from
// an HTML head <meta name="theme-color"> tag), it is blended over the dialog's
// background color (`ui::kColorDialogBackground`). Otherwise, the dialog's
// background color is returned directly.
SkColor GetEffectiveHeaderBackgroundColor(const views::View* view,
                                          std::optional<SkColor> theme_color) {
  if (!view || !view->GetWidget()) {
    return gfx::kPlaceholderColor;
  }
  const SkColor dialog_background_color =
      view->GetColorProvider()->GetColor(ui::kColorDialogBackground);
  return theme_color.has_value()
             ? color_utils::GetResultingPaintColor(theme_color.value(),
                                                   dialog_background_color)
             : dialog_background_color;
}

}  // namespace

PaymentHandlerProgressBar::PaymentHandlerProgressBar() {
  SetPreferredHeight(2);
}

PaymentHandlerProgressBar::~PaymentHandlerProgressBar() = default;

void PaymentHandlerProgressBar::SetThemeColor(
    std::optional<SkColor> theme_color) {
  theme_color_ = theme_color;
  UpdateColors();
}

void PaymentHandlerProgressBar::OnThemeChanged() {
  views::ProgressBar::OnThemeChanged();
  UpdateColors();
}

void PaymentHandlerProgressBar::UpdateColors() {
  if (!GetWidget()) {
    return;
  }
  SetColorBasedOnBackground(
      GetEffectiveHeaderBackgroundColor(this, theme_color_));
}

void PaymentHandlerProgressBar::SetColorBasedOnBackground(
    SkColor background_color) {
  // Get the closest progress bar color to kColorProgressBar, with a minimum
  // contrast ratio used for glyphs.
  const SkColor progress_bar_color = GetContrastingGoogleColor(
      gfx::kGoogleBlue600, gfx::kGoogleBlue300, background_color,
      color_utils::kMinimumVisibleContrastRatio);

  // Get the closest separator color to kColorSeparator, with a minimum
  // contrast ratio of the default light separator contrast on white, which is
  // less than color_utils::kMinimumVisibleContrastRatio.
  const SkColor separator_color = GetContrastingGoogleColor(
      gfx::kGoogleGrey300, gfx::kGoogleGrey800, background_color,
      color_utils::GetContrastRatio(gfx::kGoogleGrey300, SK_ColorWHITE));

  SetForegroundColor(progress_bar_color);
  SetBackgroundColor(separator_color);
}

base::WeakPtr<PaymentHandlerProgressBar>
PaymentHandlerProgressBar::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(PaymentHandlerProgressBar)
END_METADATA

PaymentHandlerOriginLabel::PaymentHandlerOriginLabel() {
  SetElideBehavior(gfx::ELIDE_HEAD);
  SetID(static_cast<int>(DialogViewID::SHEET_TITLE));
  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
}

PaymentHandlerOriginLabel::~PaymentHandlerOriginLabel() = default;

void PaymentHandlerOriginLabel::SetThemeColor(
    std::optional<SkColor> theme_color) {
  theme_color_ = theme_color;
  UpdateColors();
}

void PaymentHandlerOriginLabel::OnThemeChanged() {
  views::Label::OnThemeChanged();
  UpdateColors();
}

void PaymentHandlerOriginLabel::UpdateColors() {
  if (!GetWidget()) {
    return;
  }
  SetColorBasedOnBackground(
      GetEffectiveHeaderBackgroundColor(this, theme_color_));
}

void PaymentHandlerOriginLabel::SetColorBasedOnBackground(
    SkColor background_color) {
  // Get the closest label color to kColorPrimaryForeground, with a minimum
  // readable contrast ratio.
  SkColor foreground = GetContrastingGoogleColor(
      gfx::kGoogleGrey900, gfx::kGoogleGrey200, background_color,
      color_utils::kMinimumReadableContrastRatio);
  SetAutoColorReadabilityEnabled(false);
  SetEnabledColor(foreground);
  SetBackgroundColor(background_color);
}

base::WeakPtr<PaymentHandlerOriginLabel>
PaymentHandlerOriginLabel::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(PaymentHandlerOriginLabel)
END_METADATA

PaymentHandlerCloseButton::PaymentHandlerCloseButton(
    views::Button::PressedCallback pressed_callback)
    : views::ImageButton(std::move(pressed_callback)) {
  ConfigureVectorImageButton(this);
  views::InstallCircleHighlightPathGenerator(this);
  constexpr int kCloseButtonSize = 16;
  SetSize(gfx::Size(kCloseButtonSize, kCloseButtonSize));
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetID(static_cast<int>(DialogViewID::CANCEL_BUTTON));
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_PAYMENTS_CLOSE));
}

PaymentHandlerCloseButton::~PaymentHandlerCloseButton() = default;

void PaymentHandlerCloseButton::SetThemeColor(
    std::optional<SkColor> theme_color) {
  theme_color_ = theme_color;
  UpdateColors();
}

void PaymentHandlerCloseButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  UpdateColors();
}

void PaymentHandlerCloseButton::UpdateColors() {
  if (!GetWidget()) {
    return;
  }
  SetColorBasedOnBackground(
      GetEffectiveHeaderBackgroundColor(this, theme_color_));
}

void PaymentHandlerCloseButton::SetColorBasedOnBackground(
    SkColor background_color) {
  // Get the closest icon color to kColorIcon, with a minimum contrast ratio
  // used for glyphs.
  const SkColor enabled_color = GetContrastingGoogleColor(
      gfx::kGoogleGrey500, gfx::kGoogleGrey700, background_color,
      color_utils::kMinimumVisibleContrastRatio);
  const SkColor disabled_color = color_utils::AlphaBlend(
      enabled_color, background_color, gfx::kDisabledControlAlpha);

  // This view does not set its color using the browser theme color, as this
  // may differ from the header color, which is based on the web view theme.
  views::SetImageFromVectorIconWithColor(this,
                                         ::features::IsRoundedIconsEnabled()
                                             ? vector_icons::kCloseIcon
                                             : vector_icons::kCloseOldIcon,
                                         {enabled_color, disabled_color});
}

base::WeakPtr<PaymentHandlerCloseButton>
PaymentHandlerCloseButton::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(PaymentHandlerCloseButton)
END_METADATA

PaymentHandlerHeaderViews PopulatePaymentHandlerHeaderView(
    views::View* container,
    const SkBitmap* icon_bitmap,
    const std::u16string& origin_text,
    views::Button::PressedCallback close_callback) {
  // The PaymentHandler header consists of the payment app icon (if available),
  // the current web contents origin, and a close button. The origin is centered
  // on the dialog, whilst the icon and close are aligned with the LHS and RHS
  // respectively.
  //
  // +-----------------------------------------+
  // | ICON |          origin          | CLOSE |
  // +-----------------------------------------+

  container->SetID(static_cast<int>(DialogViewID::PAYMENT_APP_HEADER));
  constexpr int kVerticalInset = 8;
  constexpr int kHeaderHorizontalInset = 16;
  container->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kVerticalInset, kHeaderHorizontalInset, kVerticalInset,
                        kHeaderHorizontalInset)));

  views::TableLayout* layout =
      container->SetLayoutManager(std::make_unique<views::TableLayout>());

  // Icon column.
  const bool has_icon = icon_bitmap && !icon_bitmap->drawsNothing();
  constexpr int kHeaderIconWidth = 32;
  if (has_icon) {
    layout->AddColumn(views::LayoutAlignment::kStart,
                      views::LayoutAlignment::kCenter,
                      views::TableLayout::kFixedSize,
                      views::TableLayout::ColumnSize::kFixed, kHeaderIconWidth,
                      /*min_width=*/0);
  } else {
    layout->AddPaddingColumn(views::TableLayout::kFixedSize, kHeaderIconWidth);
  }

  // Origin column.
  layout->AddColumn(
      views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch,
      /*horizontal_resize=*/1.0, views::TableLayout::ColumnSize::kUsePreferred,
      /*fixed_width=*/0,
      /*min_width=*/0);

  // Close button column.
  layout->AddColumn(
      views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
      views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
      /*fixed_width=*/32,
      /*min_width=*/0);

  layout->AddRows(1, views::TableLayout::kFixedSize);

  // Add the icon to the header. As we support non-square icons, resize it to
  // fit the target header height.
  //
  // We should set image size in density independent pixels here, since
  // views::ImageView objects are rastered at the device scale factor.
  if (has_icon) {
    views::ImageView* app_icon_view = container->AddChildView(CreateAppIconView(
        /*icon_resource_id=*/0, icon_bitmap,
        /*tooltip_text=*/l10n_util::GetStringUTF16(IDS_PAYMENT_HANDLER_ICON)));
    app_icon_view->SetID(
        static_cast<int>(DialogViewID::PAYMENT_APP_HEADER_ICON));
    // TODO(crbug.com/40259861): If the downloaded app icon was a vector image,
    // see if we can store and rasterize it here instead of at download time.
    float adjusted_width =
        icon_bitmap->width() *
        (IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight /
         base::checked_cast<float>(icon_bitmap->height()));
    app_icon_view->SetImageSize(gfx::Size(
        adjusted_width,
        IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight));
  }

  // Add the origin label to the header.
  base::WeakPtr<PaymentHandlerOriginLabel> origin_label =
      container->AddChildView(std::make_unique<PaymentHandlerOriginLabel>())
          ->GetWeakPtr();

  // Add close button to the header.
  base::WeakPtr<PaymentHandlerCloseButton> close_button =
      container
          ->AddChildView(std::make_unique<PaymentHandlerCloseButton>(
              std::move(close_callback)))
          ->GetWeakPtr();

  return {origin_label, close_button};
}

void SetHeaderColors(views::View* header_view,
                     PaymentHandlerOriginLabel* origin_label,
                     PaymentHandlerProgressBar* progress_bar,
                     PaymentHandlerCloseButton* close_button,
                     std::optional<SkColor> theme_color) {
  // Calculates the header background based on the provided theme color if
  // provided, otherwise the Chrome theme color by default.
  header_view->SetBackground(
      theme_color.has_value() && header_view->GetWidget()
          ? views::CreateSolidBackground(color_utils::GetResultingPaintColor(
                theme_color.value(), header_view->GetColorProvider()->GetColor(
                                         ui::kColorDialogBackground)))
          : views::CreateSolidBackground(ui::kColorDialogBackground));
  if (origin_label) {
    origin_label->SetThemeColor(theme_color);
  }

  if (progress_bar) {
    progress_bar->SetThemeColor(theme_color);
  }

  if (close_button) {
    close_button->SetThemeColor(theme_color);
  }
}

}  // namespace payments
