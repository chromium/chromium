// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view_linux.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_aura_linux.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/window_frame_provider.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_background.h"
#include "ui/views/window/frame_view_utils_linux.h"

namespace {

constexpr int kFrameBorderThickness = 4;

}  // namespace

DocumentPipFrameViewLinux::DocumentPipFrameViewLinux(
    DocumentPipHost* host,
    bool supports_client_frame_shadow)
    : DocumentPipFrameView(host),
      profile_(CHECK_DEREF(host->GetProfile())),
      supports_client_frame_shadow_(supports_client_frame_shadow) {
  UpdateFrameProvider();
  top_bar_container_view()->SetBackground(nullptr);
  paint_as_active_subscription_ =
      host->GetWidget()->RegisterPaintAsActiveChangedCallback(
          base::BindRepeating(&views::View::SchedulePaint,
                              base::Unretained(this)));
}

DocumentPipFrameViewLinux::~DocumentPipFrameViewLinux() = default;

void DocumentPipFrameViewLinux::UpdateFrameProvider() {
  window_frame_provider_ = nullptr;
  auto* linux_ui_theme =
      ThemeServiceAuraLinux::GetLinuxUiThemeForProfile(&profile_.get());
  if (linux_ui_theme &&
      ThemeServiceFactory::GetForProfile(&profile_.get())->UsingSystemTheme()) {
    window_frame_provider_ = linux_ui_theme->GetWindowFrameProvider(
        ui::FrameType::kBrowser, !supports_client_frame_shadow_, tiled_,
        /*maximized=*/false);
  }
  frame_background_ = window_frame_provider_
                          ? nullptr
                          : std::make_unique<views::FrameBackground>();
}

void DocumentPipFrameViewLinux::OnThemeChanged() {
  UpdateFrameProvider();
  DocumentPipFrameView::OnThemeChanged();
  InvalidateLayout();
}

gfx::ShadowValues DocumentPipFrameViewLinux::GetShadowValues() {
  return gfx::ShadowValue::MakeMdShadowValues(
      ChromeLayoutProvider::Get()->GetShadowElevationMetric(
          views::Emphasis::kMaximum));
}

gfx::Insets DocumentPipFrameViewLinux::RestoredMirroredFrameBorderInsets()
    const {
  const auto border = FrameBorderInsets();
  return base::i18n::IsRTL() ? gfx::Insets::TLBR(border.top(), border.right(),
                                                 border.bottom(), border.left())
                             : border;
}

gfx::Insets DocumentPipFrameViewLinux::GetInputInsets() const {
  return supports_client_frame_shadow_ ? ResizeBorderInsets() : gfx::Insets();
}

SkRRect DocumentPipFrameViewLinux::GetRestoredClipRegion() const {
  const gfx::InsetsF border =
      supports_client_frame_shadow_
          ? gfx::InsetsF(RestoredMirroredFrameBorderInsets())
          : gfx::InsetsF();
  const float radius = window_frame_provider_
                           ? window_frame_provider_->GetTopCornerRadiusDip()
                           : ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
                                 views::Emphasis::kHigh);
  return views::GetRestoredClipRegion(
      gfx::RectF(GetLocalBounds()), border,
      gfx::RoundedCornersF(radius, radius, 0, 0));
}

gfx::Rect DocumentPipFrameViewLinux::GetNonDecoratedClientAreaBoundsInScreen()
    const {
  gfx::Rect bounds = GetBoundsInScreen();
  if (supports_client_frame_shadow_) {
    bounds.Inset(FrameBorderInsets());
  }
  return bounds;
}

void DocumentPipFrameViewLinux::SetTiled(bool tiled) {
  if (tiled_ == tiled) {
    return;
  }
  tiled_ = tiled;
  UpdateFrameProvider();
  InvalidateLayout();
  SchedulePaint();
}

void DocumentPipFrameViewLinux::OnPaint(gfx::Canvas* canvas) {
  if (window_frame_provider_) {
    window_frame_provider_->PaintWindowFrame(
        canvas, GetLocalBounds(), GetTopAreaHeight(), ShouldPaintAsActive(),
        GetInputInsets());
  } else {
    const ui::ThemeProvider& theme_provider = CHECK_DEREF(GetThemeProvider());
    const int image_id =
        ShouldPaintAsActive() ? IDR_THEME_FRAME : IDR_THEME_FRAME_INACTIVE;
    frame_background_->set_frame_color(
        GetColorProvider()->GetColor(kColorPipWindowTopBarBackground));
    frame_background_->set_use_custom_frame(true);
    frame_background_->set_is_active(ShouldPaintAsActive());
    frame_background_->set_theme_image(
        theme_provider.HasCustomImage(image_id) ||
                theme_provider.HasCustomImage(IDR_THEME_FRAME)
            ? *theme_provider.GetImageSkiaNamed(image_id)
            : gfx::ImageSkia());
    const gfx::Rect client_bounds = GetBoundsForClientView();
    frame_background_->set_theme_image_inset(
        gfx::Point(-client_bounds.x(),
                   ThemeProperties::kFrameHeightAboveTabs - client_bounds.y()));
    frame_background_->set_top_area_height(GetTopAreaHeight());
    views::PaintRestoredFrameBorderLinux(
        *canvas, *this, frame_background_.get(), GetRestoredClipRegion(),
        supports_client_frame_shadow_, ShouldPaintAsActive(),
        RestoredMirroredFrameBorderInsets(), GetShadowValues(), tiled_);
  }
  views::FrameView::OnPaint(canvas);
}

gfx::Insets DocumentPipFrameViewLinux::FrameBorderInsets() const {
  if (window_frame_provider_) {
    return tiled_ ? gfx::Insets()
                  : window_frame_provider_->GetFrameThicknessDip();
  }
  return views::GetRestoredFrameBorderInsetsLinux(
      supports_client_frame_shadow_, gfx::Insets(kFrameBorderThickness),
      GetShadowValues(), DocumentPipFrameView::ResizeBorderInsets());
}

gfx::Insets DocumentPipFrameViewLinux::ResizeBorderInsets() const {
  return FrameBorderInsets();
}

BEGIN_METADATA(DocumentPipFrameViewLinux)
END_METADATA
