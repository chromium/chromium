// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/windows_caption_button.h"
#include <memory>

#include "base/numerics/safe_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/frame/window_frame_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_win.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"

WindowsCaptionButton::WindowsCaptionButton(
    PressedCallback callback,
    BrowserFrameViewWin* frame_view,
    ViewID button_type,
    const std::u16string& accessible_name)
    : views::Button(std::move(callback)),
      frame_view_(frame_view),
      icon_painter_(CreateIconPainter()),
      button_type_(button_type) {
  SetAnimateOnStateChange(true);
  // Not focusable by default, only for accessibility.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetName(accessible_name);
  SetID(button_type);
}

WindowsCaptionButton::~WindowsCaptionButton() = default;

std::unique_ptr<Windows10IconPainter>
WindowsCaptionButton::CreateIconPainter() {
  if (base::win::GetVersion() >= base::win::Version::WIN11) {
    return std::make_unique<Windows11IconPainter>();
  }
  return std::make_unique<Windows10IconPainter>();
}

gfx::Size WindowsCaptionButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int width =
      WindowFrameUtil::kWindowsCaptionButtonWidth + GetBetweenButtonSpacing();

  // TODO(bsep): The sizes in this function are for 1x device scale and don't
  // match Windows button sizes at hidpi.
  int height = WindowFrameUtil::kWindowsCaptionButtonHeightRestored;
  if (!frame_view_->browser_view()->webui_tab_strip() &&
      frame_view_->IsMaximized()) {
    int maximized_height =
        frame_view_->browser_view()->ShouldDrawTabStrip()
            ? frame_view_->browser_view()->GetTabStripHeight()
            : frame_view_->TitlebarMaximizedVisualHeight();
    constexpr int kMaximizedBottomMargin = 2;
    maximized_height -= kMaximizedBottomMargin;
    height = std::min(height, maximized_height);
  }
  return gfx::Size(width, height);
}

SkColor WindowsCaptionButton::GetBaseForegroundColor() const {
  return GetColorProvider()->GetColor(
      frame_view_->ShouldPaintAsActive()
          ? kColorCaptionButtonForegroundActive
          : kColorCaptionButtonForegroundInactive);
}

void WindowsCaptionButton::OnPaintBackground(gfx::Canvas* canvas) {
  // Paint the background of the button (the semi-transparent rectangle that
  // appears when you hover or press the button).
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  const SkColor bg_color =
      GetColorProvider()->GetColor(kColorCaptionButtonBackground);
  const SkAlpha theme_alpha = SkColorGetA(bg_color);
  gfx::Rect bounds = GetContentsBounds();
  bounds.Inset(gfx::Insets::TLBR(0, GetBetweenButtonSpacing(), 0, 0));

  if (theme_alpha > 0) {
    canvas->FillRect(
        bounds,
        SkColorSetA(
            bg_color,
            WindowFrameUtil::CalculateWindowsCaptionButtonBackgroundAlpha(
                theme_alpha)));
  }
  if (theme_provider->HasCustomImage(IDR_THEME_WINDOW_CONTROL_BACKGROUND)) {
    // Figure out what portion of the background image to display
    const int button_display_order = GetButtonDisplayOrderIndex();
    const int base_button_width = WindowFrameUtil::kWindowsCaptionButtonWidth;
    const int base_visual_spacing =
        WindowFrameUtil::kWindowsCaptionButtonVisualSpacing;
    const int src_x =
        button_display_order * (base_button_width + base_visual_spacing);
    const int src_y = 0;

    canvas->TileImageInt(
        *theme_provider->GetImageSkiaNamed(IDR_THEME_WINDOW_CONTROL_BACKGROUND),
        src_x, src_y, bounds.x(), bounds.y(), bounds.width(), bounds.height());
  }

  SkColor base_color;
  SkAlpha hovered_alpha, pressed_alpha;
  if (button_type_ == VIEW_ID_CLOSE_BUTTON) {
    base_color =
        GetColorProvider()->GetColor(kColorCaptionCloseButtonBackgroundHovered);
    hovered_alpha = SK_AlphaOPAQUE;
    pressed_alpha = 0x98;
  } else {
    // Match the native buttons.
    base_color = GetBaseForegroundColor();
    hovered_alpha = 0x1A;
    pressed_alpha = 0x33;

    if (theme_alpha > 0) {
      // Theme buttons have slightly increased opacity to make them stand out
      // against a visually-busy frame image.
      constexpr float kAlphaScale = 1.3f;
      hovered_alpha = base::ClampRound<SkAlpha>(hovered_alpha * kAlphaScale);
      pressed_alpha = base::ClampRound<SkAlpha>(pressed_alpha * kAlphaScale);
    }
  }

  SkAlpha alpha;
  if (GetState() == STATE_PRESSED)
    alpha = pressed_alpha;
  else
    alpha = gfx::Tween::IntValueBetween(hover_animation().GetCurrentValue(),
                                        SK_AlphaTRANSPARENT, hovered_alpha);
  canvas->FillRect(bounds, SkColorSetA(base_color, alpha));
}

void WindowsCaptionButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintSymbol(canvas);
}

int WindowsCaptionButton::GetBetweenButtonSpacing() const {
  const int display_order_index = GetButtonDisplayOrderIndex();
  return display_order_index == 0
             ? 0
             : WindowFrameUtil::kWindowsCaptionButtonVisualSpacing;
}

int WindowsCaptionButton::GetButtonDisplayOrderIndex() const {
  int button_display_order = 0;
  switch (button_type_) {
    case VIEW_ID_MINIMIZE_BUTTON:
      button_display_order = 0;
      break;
    case VIEW_ID_MAXIMIZE_BUTTON:
    case VIEW_ID_RESTORE_BUTTON:
      button_display_order = 1;
      break;
    case VIEW_ID_CLOSE_BUTTON:
      button_display_order = 2;
      break;
    default:
      NOTREACHED();
  }

  // Reverse the ordering if we're in RTL mode
  if (base::i18n::IsRTL()) {
    const int max_index = 2;
    button_display_order = max_index - button_display_order;
  }

  return button_display_order;
}

void WindowsCaptionButton::PaintSymbol(gfx::Canvas* canvas) {
  SkColor symbol_color = GetBaseForegroundColor();
  const SkColor hovered_color =
      GetColorProvider()->GetColor(kColorCaptionCloseButtonForegroundHovered);
  if (!GetEnabled() ||
      (!frame_view_->ShouldPaintAsActive() && GetState() != STATE_HOVERED &&
       GetState() != STATE_PRESSED)) {
    symbol_color =
        SkColorSetA(symbol_color, SkColorGetA(GetColorProvider()->GetColor(
                                      kColorCaptionForegroundInactive)));
  } else if (button_type_ == VIEW_ID_CLOSE_BUTTON &&
             hover_animation().is_animating()) {
    symbol_color = gfx::Tween::ColorValueBetween(
        hover_animation().GetCurrentValue(), symbol_color, hovered_color);
  } else if (button_type_ == VIEW_ID_CLOSE_BUTTON &&
             (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED)) {
    symbol_color = hovered_color;
  }

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  const int symbol_size_pixels = base::ClampRound(10 * scale);
  gfx::RectF bounds_rect(GetContentsBounds());
  bounds_rect.Scale(scale);
  gfx::Rect symbol_rect(gfx::ToEnclosingRect(bounds_rect));
  symbol_rect.ClampToCenteredSize(
      gfx::Size(symbol_size_pixels, symbol_size_pixels));

  cc::PaintFlags flags;
  flags.setAntiAlias(false);
  flags.setColor(symbol_color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  const int stroke_width = base::ClampRound(scale);
  flags.setStrokeWidth(stroke_width);

  switch (button_type_) {
    case VIEW_ID_MINIMIZE_BUTTON:
      icon_painter_->PaintMinimizeIcon(canvas, symbol_rect, flags);
      return;

    case VIEW_ID_MAXIMIZE_BUTTON:
      icon_painter_->PaintMaximizeIcon(canvas, symbol_rect, flags);
      return;

    case VIEW_ID_RESTORE_BUTTON:
      icon_painter_->PaintRestoreIcon(canvas, symbol_rect, flags);
      return;

    case VIEW_ID_CLOSE_BUTTON: {
      // The close button's X is surrounded by a "halo" of transparent pixels.
      // When the X is white, the transparent pixels need to be a bit brighter
      // to be visible.
      const float stroke_halo =
          stroke_width * (symbol_color == hovered_color ? 0.1f : 0.05f);
      flags.setStrokeWidth(stroke_width + stroke_halo);

      icon_painter_->PaintCloseIcon(canvas, symbol_rect, flags);
      return;
    }

    default:
      NOTREACHED();
  }
}

BEGIN_METADATA(WindowsCaptionButton)
ADD_READONLY_PROPERTY_METADATA(int, BetweenButtonSpacing)
ADD_READONLY_PROPERTY_METADATA(int, ButtonDisplayOrderIndex)
ADD_READONLY_PROPERTY_METADATA(SkColor,
                               BaseForegroundColor,
                               ui::metadata::SkColorConverter)
END_METADATA
