// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_region_select_overlay.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#endif

#if defined(USE_AURA)
#include "ui/wm/core/window_animations.h"
#endif

namespace omnibox_everywhere {

namespace {

using RegionCaptureSource =
    OmniboxEverywhereRegionSelectOverlay::RegionCaptureSource;

// Lens dark slate scrim.
constexpr SkColor kChromnientSlateScrim =
    SkColorSetA(SkColorSetRGB(0x18, 0x1C, 0x22), 165);

constexpr float kSelectionRectStrokeWidth = 2.5f;

constexpr float kGlifGradientWashAlpha = 0.18f;

SkColor4f ColorWithAlpha(SkColor color, float alpha) {
  SkColor4f c = SkColor4f::FromColor(color);
  c.fA = alpha;
  return c;
}

gfx::Rect GetDisplayPhysicalBounds(const display::Display& display) {
#if BUILDFLAG(IS_WIN)
  // Queries Windows's raw monitor pixel bounds to avoid layout drift
  // and rounding seams when converting mixed-DPI displays to/from DIPs.
  const display::win::ScreenWin* screen_win = display::win::GetScreenWin();
  if (screen_win && display::Screen::Get() == screen_win) {
    const auto sw_display =
        screen_win->GetScreenWinDisplayWithDisplayId(display.id());
    if (sw_display.display().id() == display.id() &&
        !sw_display.pixel_bounds().IsEmpty()) {
      return sw_display.pixel_bounds();
    }
  }
#endif
  return gfx::ScaleToEnclosingRect(display.bounds(),
                                   display.device_scale_factor());
}

gfx::Rect GetVirtualDesktopPixelBounds(
    const std::vector<display::Display>& displays) {
  gfx::Rect total;
  for (const auto& d : displays) {
    total.Union(GetDisplayPhysicalBounds(d));
  }
  return total;
}

std::vector<display::Display> GetTargetDisplaysForSource(
    const RegionCaptureSource& source) {
  auto* screen = display::Screen::Get();
  if (!screen) {
    return {};
  }

  const auto all_displays = screen->GetAllDisplays();
  if (all_displays.empty()) {
    return {};
  }

  // Single display (e.g. macOS screen picker or display-specific capture).
  if (source.type == RegionCaptureSource::Type::kSpecificDisplay &&
      source.display_id) {
    display::Display display;
    if (screen->GetDisplayWithDisplayId(*source.display_id, &display)) {
      return {display};
    }
    // Fall back to primary display if specified display was disconnected.
    return {screen->GetPrimaryDisplay()};
  }

  // Full virtual desktop (Windows / Linux full desktop capture).
  if (source.type == RegionCaptureSource::Type::kAllDisplays) {
    return all_displays;
  }

  return {screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint())};
}

class InstructionToastChipView : public views::View {
  METADATA_HEADER(InstructionToastChipView, views::View)

 public:
  InstructionToastChipView() {
    SetCanProcessEventsWithinSubtree(false);
    GetViewAccessibility().SetIsIgnored(true);

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(0, 16),
        /*between_child_spacing=*/8));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    constexpr SkColor kForegroundColor = SkColorSetRGB(0xEE, 0xF0, 0xF9);
    constexpr int kIconSize = 20;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    const gfx::VectorIcon& icon = vector_icons::kGoogleLensMonochromeLogoIcon;
#else
    const gfx::VectorIcon& icon = vector_icons::kSearchIcon;
#endif

    auto icon_view = std::make_unique<views::ImageView>();
    icon_view->SetImage(
        ui::ImageModel::FromVectorIcon(icon, kForegroundColor, kIconSize));
    AddChildView(std::move(icon_view));

    auto label = std::make_unique<views::Label>(l10n_util::GetStringUTF16(
        IDS_LENS_OVERLAY_INITIAL_TOAST_MESSAGE_SIMPLIFIED));
    label->SetEnabledColor(kForegroundColor);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetSubpixelRenderingEnabled(false);
    AddChildView(std::move(label));
  }

  InstructionToastChipView(const InstructionToastChipView&) = delete;
  InstructionToastChipView& operator=(const InstructionToastChipView&) = delete;
  ~InstructionToastChipView() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size size = views::View::CalculatePreferredSize(available_size);
    return gfx::Size(size.width(), 40);
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    gfx::RectF chip_rect(GetLocalBounds());
    cc::PaintFlags fill_flags;
    fill_flags.setColor(SkColorSetA(SkColorSetRGB(0x18, 0x1C, 0x22), 220));
    fill_flags.setStyle(cc::PaintFlags::kFill_Style);
    fill_flags.setAntiAlias(true);
    canvas->DrawRoundRect(chip_rect, height() * 0.5f, fill_flags);
  }

  void OnPaintBorder(gfx::Canvas* canvas) override {
    gfx::RectF border_rect(GetLocalBounds());
    border_rect.Inset(0.5f);
    cc::PaintFlags border_flags;
    border_flags.setStyle(cc::PaintFlags::kStroke_Style);
    border_flags.setStrokeWidth(1.0f);
    border_flags.setColor(SkColorSetA(SK_ColorWHITE, 40));
    border_flags.setAntiAlias(true);
    canvas->DrawRoundRect(border_rect, border_rect.height() * 0.5f,
                          border_flags);
  }
};

BEGIN_METADATA(InstructionToastChipView)
END_METADATA

class TeardropCursorChipView : public views::View {
  METADATA_HEADER(TeardropCursorChipView, views::View)

 public:
  static constexpr int kChipSize = 32;
  static constexpr int kPadding = 12;
  static constexpr int kTotalSize = kChipSize + (kPadding * 2);

  TeardropCursorChipView() {
    SetCanProcessEventsWithinSubtree(false);
    GetViewAccessibility().SetIsIgnored(true);

    shadow_fill_flags_.setAntiAlias(true);
    shadow_fill_flags_.setStyle(cc::PaintFlags::kFill_Style);
    shadow_fill_flags_.setColor(SK_ColorWHITE);
    shadow_fill_flags_.setImageFilter(sk_make_sp<cc::DropShadowPaintFilter>(
        0.0f, 2.0f, 4.0f, 4.0f, SkColor4f{0.0f, 0.0f, 0.0f, 0.24f},
        cc::DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground,
        nullptr));

    border_flags_.setStyle(cc::PaintFlags::kStroke_Style);
    border_flags_.setStrokeWidth(1.0f);
    border_flags_.setColor(SkColorSetA(SK_ColorBLACK, 25));
    border_flags_.setAntiAlias(true);
  }

  TeardropCursorChipView(const TeardropCursorChipView&) = delete;
  TeardropCursorChipView& operator=(const TeardropCursorChipView&) = delete;
  ~TeardropCursorChipView() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kTotalSize, kTotalSize);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    // Asymmetrical teardrop chip (radii: 4px top-left, 16px other 3
    // corners).
    const SkVector radii[4] = {SkVector::Make(4, 4), SkVector::Make(16, 16),
                               SkVector::Make(16, 16), SkVector::Make(16, 16)};
    SkRRect teardrop_rrect = SkRRect::MakeRectRadii(
        SkRect::MakeXYWH(kPadding, kPadding, kChipSize, kChipSize), radii);
    SkPath teardrop_path = SkPath::RRect(teardrop_rrect);

    canvas->DrawPath(teardrop_path, shadow_fill_flags_);
    canvas->DrawPath(teardrop_path, border_flags_);

    // Magnifying glass icon.
    constexpr int kIconSize = 16;
    const int icon_x = kPadding + (kChipSize - kIconSize) / 2;
    const int icon_y = kPadding + (kChipSize - kIconSize) / 2;
    canvas->Save();
    canvas->Translate(gfx::Vector2d(icon_x, icon_y));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    const gfx::VectorIcon& icon = vector_icons::kGoogleLensMonochromeLogoIcon;
#else
    const gfx::VectorIcon& icon = vector_icons::kSearchIcon;
#endif
    gfx::PaintVectorIcon(canvas, icon, kIconSize,
                         SkColorSetRGB(0x1F, 0x1F, 0x1F));
    canvas->Restore();
  }

 private:
  cc::PaintFlags shadow_fill_flags_;
  cc::PaintFlags border_flags_;
};

BEGIN_METADATA(TeardropCursorChipView)
END_METADATA

class RegionSelectOverlayView : public views::View {
  METADATA_HEADER(RegionSelectOverlayView, views::View)

 public:
  using ConfirmCallback = base::OnceCallback<void(const SkBitmap& cropped)>;

  RegionSelectOverlayView(const SkBitmap& screenshot,
                          ConfirmCallback on_confirm,
                          base::OnceClosure on_cancel)
      : bitmap_(screenshot),
        image_(!bitmap_.empty() ? gfx::ImageSkia::CreateFromBitmap(bitmap_, 1.f)
                                : gfx::ImageSkia()),
        on_confirm_(std::move(on_confirm)),
        on_cancel_(std::move(on_cancel)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    GetViewAccessibility().SetRole(ax::mojom::Role::kImage);
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_OMNIBOX_EVERYWHERE_REGION_SELECT_ACCESSIBLE_NAME));
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

    toast_chip_ = AddChildView(std::make_unique<InstructionToastChipView>());
    cursor_chip_ = AddChildView(std::make_unique<TeardropCursorChipView>());
    cursor_chip_->SetVisible(false);
  }

  RegionSelectOverlayView(const RegionSelectOverlayView&) = delete;
  RegionSelectOverlayView& operator=(const RegionSelectOverlayView&) = delete;
  ~RegionSelectOverlayView() override = default;

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::Cursor(ui::mojom::CursorType::kCross);
  }

  const SkBitmap& bitmap_for_testing() const { return bitmap_; }

  void AddedToWidget() override {
    views::View::AddedToWidget();
    UpdateToastPosition();
  }

  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    views::View::OnBoundsChanged(previous_bounds);
    UpdateToastPosition();
  }

  void DrawScreenshotImage(gfx::Canvas* canvas) {
    CHECK(!image_.isNull());
    canvas->DrawImageInt(image_, /*src_x=*/0, /*src_y=*/0,
                         /*src_w=*/image_.width(), /*src_h=*/image_.height(),
                         /*dest_x=*/0, /*dest_y=*/0,
                         /*dest_w=*/width(), /*dest_h=*/height(),
                         /*filter=*/true);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    if (bitmap_.empty() || image_.isNull() || width() <= 0 || height() <= 0) {
      canvas->DrawColor(SK_ColorBLACK);
      return;
    }

    // 1. Draw base un-dimmed screenshot.
    DrawScreenshotImage(canvas);

    // 2. Apply dark scrim over the screen.
    canvas->FillRect(GetLocalBounds(), kChromnientSlateScrim);

    // 3. GLIF rainbow gradient wash.
    DrawRainbowGradientWash(canvas);

    if (!selection_rect_.IsEmpty()) {
      // 4. Selection perimeter.
      DrawSelectionRegion(canvas);
    }
  }

  void OnMouseMoved(const ui::MouseEvent& event) override {
    UpdateCursorChipPosition(event.location());
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    UpdateCursorChipPosition(event.location());
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    if (cursor_chip_) {
      cursor_chip_->SetVisible(false);
    }
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton()) {
      is_dragging_ = true;
      drag_start_ = event.location();
      selection_rect_ = gfx::Rect(drag_start_, gfx::Size(0, 0));
      if (cursor_chip_) {
        cursor_chip_->SetVisible(false);
      }
      if (toast_chip_) {
        toast_chip_->SetVisible(false);
      }
      SchedulePaint();
      return true;
    }
    return views::View::OnMousePressed(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (is_dragging_) {
      UpdateSelectionRect(gfx::BoundingRect(drag_start_, event.location()));
      return true;
    }
    return views::View::OnMouseDragged(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (is_dragging_ && event.IsLeftMouseButton()) {
      is_dragging_ = false;
      CompleteSelection();
    }
  }

  void OnMouseCaptureLost() override {
    if (is_dragging_) {
      is_dragging_ = false;
      Cancel();
    }
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    switch (event->type()) {
      case ui::EventType::kGestureTapDown:
      case ui::EventType::kGestureTapCancel:
        event->SetHandled();
        break;
      case ui::EventType::kGestureScrollBegin:
        is_dragging_ = true;
        drag_start_ = event->location();
        selection_rect_ = gfx::Rect(drag_start_, gfx::Size(0, 0));
        if (cursor_chip_) {
          cursor_chip_->SetVisible(false);
        }
        if (toast_chip_) {
          toast_chip_->SetVisible(false);
        }
        SchedulePaint();
        event->SetHandled();
        break;
      case ui::EventType::kGestureScrollUpdate:
        if (is_dragging_) {
          UpdateSelectionRect(
              gfx::BoundingRect(drag_start_, event->location()));
          event->SetHandled();
        }
        break;
      case ui::EventType::kGestureScrollEnd:
      case ui::EventType::kGestureEnd:
        if (is_dragging_) {
          is_dragging_ = false;
          CompleteSelection();
          event->SetHandled();
        }
        break;
      default:
        break;
    }
  }

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    if (accelerator.key_code() == ui::VKEY_ESCAPE) {
      Cancel();
      return true;
    }
    return false;
  }

 private:
  void UpdateToastPosition() {
    if (!toast_chip_ || width() <= 0 || height() <= 0) {
      return;
    }
    const gfx::Size toast_size = toast_chip_->GetPreferredSize();
    // On macOS, provide extra top clearance to account for the system menu bar
    // and MacBook display notches (which occupy up to ~44pt at the top). On
    // other platforms, y = 0 is clear of system chrome.
#if BUILDFLAG(IS_MAC)
    constexpr int kTopMargin = 56;
#else
    constexpr int kTopMargin = 28;
#endif
    const int x = std::max(0, (width() - toast_size.width()) / 2);
    toast_chip_->SetBounds(x, kTopMargin, toast_size.width(),
                           toast_size.height());
  }

  void UpdateCursorChipPosition(const gfx::Point& pos) {
    if (!cursor_chip_) {
      return;
    }
    if (is_dragging_) {
      cursor_chip_->SetVisible(false);
      return;
    }

    constexpr int kOffset = 8;
    int chip_x = pos.x() + kOffset;
    int chip_y = pos.y() + kOffset;

    if (chip_x + TeardropCursorChipView::kTotalSize > width()) {
      chip_x = pos.x() - TeardropCursorChipView::kTotalSize - kOffset;
    }
    if (chip_y + TeardropCursorChipView::kTotalSize > height()) {
      chip_y = pos.y() - TeardropCursorChipView::kTotalSize - kOffset;
    }
    chip_x = std::clamp(
        chip_x, 0, std::max(0, width() - TeardropCursorChipView::kTotalSize));
    chip_y = std::clamp(
        chip_y, 0, std::max(0, height() - TeardropCursorChipView::kTotalSize));

    cursor_chip_->SetBounds(chip_x, chip_y, TeardropCursorChipView::kTotalSize,
                            TeardropCursorChipView::kTotalSize);
    cursor_chip_->SetVisible(true);
  }

  void DrawRainbowGradientWash(gfx::Canvas* canvas) {
    if (width() <= 0 || height() <= 0) {
      return;
    }
    // GLIF rainbow gradient.
    const SkColor4f kGlifColors[] = {
        ColorWithAlpha(gfx::kGoogleBlue500, kGlifGradientWashAlpha),
        ColorWithAlpha(gfx::kGoogleRed500, kGlifGradientWashAlpha),
        ColorWithAlpha(gfx::kGoogleYellow500, kGlifGradientWashAlpha),
        ColorWithAlpha(gfx::kGoogleGreen500, kGlifGradientWashAlpha),
        ColorWithAlpha(gfx::kGoogleBlue500, kGlifGradientWashAlpha),
    };
    SkPoint points[2] = {SkPoint::Make(0, 0), SkPoint::Make(width(), height())};
    cc::PaintFlags gradient_flags;
    gradient_flags.setAntiAlias(true);
    gradient_flags.setStyle(cc::PaintFlags::kFill_Style);
    gradient_flags.setShader(cc::PaintShader::MakeLinearGradient(
        points, kGlifColors, nullptr, std::size(kGlifColors),
        SkTileMode::kClamp));
    canvas->DrawRect(gfx::RectF(GetLocalBounds()), gradient_flags);
  }

  void DrawSelectionRegion(gfx::Canvas* canvas) {
    if (selection_rect_.IsEmpty()) {
      return;
    }

    constexpr float kIdealCornerRadius = 14.0f;
    const float corner_radius =
        std::min({kIdealCornerRadius, selection_rect_.width() / 2.0f,
                  selection_rect_.height() / 2.0f});

    SkRect sk_sel_rect =
        SkRect::MakeXYWH(selection_rect_.x(), selection_rect_.y(),
                         selection_rect_.width(), selection_rect_.height());
    SkPath sel_path = SkPath::RRect(
        SkRRect::MakeRectXY(sk_sel_rect, corner_radius, corner_radius));

    // Re-draw full-clarity screenshot inside rounded selection path.
    canvas->Save();
    canvas->ClipPath(sel_path, true);
    DrawScreenshotImage(canvas);
    canvas->Restore();

    // Perimeter border with rounded corners.
    cc::PaintFlags stroke_flags;
    stroke_flags.setColor(SK_ColorWHITE);
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setStrokeWidth(kSelectionRectStrokeWidth);
    stroke_flags.setAntiAlias(true);
    canvas->DrawPath(sel_path, stroke_flags);
  }

  void UpdateSelectionRect(const gfx::Rect& new_rect) {
    if (new_rect == selection_rect_) {
      return;
    }
    gfx::Rect damage_rect = gfx::UnionRects(selection_rect_, new_rect);
    // Expand by stroke width plus anti-aliasing margin so the perimeter
    // border is completely cleared and redrawn.
    constexpr float kDamageRectOutset = kSelectionRectStrokeWidth + 0.5f;
    damage_rect.Outset(base::ClampCeil(kDamageRectOutset));
    selection_rect_ = new_rect;
    SchedulePaintInRect(damage_rect);
  }

  void Cancel() {
    if (on_cancel_) {
      std::move(on_cancel_).Run();
    }
  }

  void CompleteSelection() {
    constexpr int kMinSelectionSize = 10;
    if (selection_rect_.width() >= kMinSelectionSize &&
        selection_rect_.height() >= kMinSelectionSize) {
      const float scale_x =
          width() > 0 ? static_cast<float>(bitmap_.width()) / width() : 1.0f;
      const float scale_y =
          height() > 0 ? static_cast<float>(bitmap_.height()) / height() : 1.0f;
      gfx::Rect scaled_rect(
          static_cast<int>(std::round(selection_rect_.x() * scale_x)),
          static_cast<int>(std::round(selection_rect_.y() * scale_y)),
          static_cast<int>(std::round(selection_rect_.width() * scale_x)),
          static_cast<int>(std::round(selection_rect_.height() * scale_y)));
      scaled_rect.Intersect(gfx::Rect(bitmap_.width(), bitmap_.height()));

      if (scaled_rect.width() > 0 && scaled_rect.height() > 0) {
        SkIRect sk_crop =
            SkIRect::MakeXYWH(scaled_rect.x(), scaled_rect.y(),
                              scaled_rect.width(), scaled_rect.height());
        SkBitmap cropped;
        if (bitmap_.extractSubset(&cropped, sk_crop)) {
          if (on_confirm_) {
            std::move(on_confirm_).Run(cropped);
          }
          return;
        }
      }
    }
    Cancel();
  }

  SkBitmap bitmap_;
  gfx::ImageSkia image_;
  ConfirmCallback on_confirm_;
  base::OnceClosure on_cancel_;
  raw_ptr<InstructionToastChipView> toast_chip_ = nullptr;
  raw_ptr<TeardropCursorChipView> cursor_chip_ = nullptr;
  gfx::Point drag_start_;
  gfx::Rect selection_rect_;
  bool is_dragging_ = false;
};

BEGIN_METADATA(RegionSelectOverlayView)
END_METADATA

}  // namespace

// static
std::unique_ptr<OmniboxEverywhereRegionSelectOverlay>
OmniboxEverywhereRegionSelectOverlay::Create(const SkBitmap& screenshot,
                                             const RegionCaptureSource& source,
                                             CompleteCallback callback,
                                             gfx::NativeWindow context) {
  auto overlay = base::WrapUnique(
      new OmniboxEverywhereRegionSelectOverlay(std::move(callback)));
  overlay->Initialize(screenshot, source, context);
  return overlay;
}

OmniboxEverywhereRegionSelectOverlay::OmniboxEverywhereRegionSelectOverlay(
    CompleteCallback callback)
    : callback_(std::move(callback)) {}

OmniboxEverywhereRegionSelectOverlay::~OmniboxEverywhereRegionSelectOverlay() {
  widget_observations_.RemoveAllObservations();
  for (auto& widget : widgets_) {
    if (widget && !widget->IsClosed()) {
      widget->CloseNow();
    }
  }
  widgets_.clear();
  if (callback_) {
    std::move(callback_).Run(SkBitmap());
  }
}

size_t OmniboxEverywhereRegionSelectOverlay::GetActiveWidgetIndex() const {
  if (widgets_.empty()) {
    return 0;
  }
  auto* screen = display::Screen::Get();
  if (screen) {
    const gfx::Point cursor = screen->GetCursorScreenPoint();
    for (size_t i = 0; i < widgets_.size(); ++i) {
      if (widgets_[i] &&
          widgets_[i]->GetWindowBoundsInScreen().Contains(cursor)) {
        return i;
      }
    }
  }
  return 0;
}

views::Widget*
OmniboxEverywhereRegionSelectOverlay::GetActiveWidgetForTesting() {
  return widgets_.empty() ? nullptr : widgets_[GetActiveWidgetIndex()].get();
}

const views::Widget*
OmniboxEverywhereRegionSelectOverlay::GetActiveWidgetForTesting() const {
  return widgets_.empty() ? nullptr : widgets_[GetActiveWidgetIndex()].get();
}

const SkBitmap&
OmniboxEverywhereRegionSelectOverlay::GetBitmapForWidgetForTesting(
    size_t widget_index) const {
  CHECK_LT(widget_index, widgets_.size());
  auto* view = static_cast<const RegionSelectOverlayView*>(
      widgets_[widget_index]->GetContentsView());
  return view->bitmap_for_testing();  // IN-TEST
}

void OmniboxEverywhereRegionSelectOverlay::Initialize(
    const SkBitmap& screenshot,
    const RegionCaptureSource& source,
    gfx::NativeWindow context) {
  std::vector<display::Display> target_displays =
      GetTargetDisplaysForSource(source);
  if (target_displays.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&OmniboxEverywhereRegionSelectOverlay::Finish,
                                  weak_factory_.GetWeakPtr(), SkBitmap()));
    return;
  }

  const gfx::Rect total_pixel_bounds =
      GetVirtualDesktopPixelBounds(target_displays);
  const gfx::Point virtual_origin = total_pixel_bounds.origin();
  const gfx::Rect screenshot_bounds(0, 0, screenshot.width(),
                                    screenshot.height());

  // Determine which display should be active/focused (display nearest cursor).
  auto* screen = display::Screen::Get();
  int64_t active_display_id =
      screen
          ? screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint()).id()
          : target_displays[0].id();

  views::Widget* active_widget = nullptr;

  for (const auto& display : target_displays) {
    gfx::Rect sub_rect;
    if (target_displays.size() == 1) {
      sub_rect = screenshot_bounds;
    } else {
      const gfx::Rect physical_bounds = GetDisplayPhysicalBounds(display);
      sub_rect = physical_bounds - virtual_origin.OffsetFromOrigin();
    }

    gfx::Rect crop_rect = gfx::IntersectRects(sub_rect, screenshot_bounds);

    SkBitmap display_bitmap;
    if (!crop_rect.IsEmpty()) {
      screenshot.extractSubset(
          &display_bitmap,
          SkIRect::MakeXYWH(crop_rect.x(), crop_rect.y(), crop_rect.width(),
                            crop_rect.height()));
    }

    std::unique_ptr<views::Widget> widget =
        CreateWidgetForDisplay(display, display_bitmap, context);

    if (display.id() == active_display_id) {
      active_widget = widget.get();
    }

    widgets_.push_back(std::move(widget));
  }

  // Fallback to the first display's widget if the cursor is not located within
  // any target display.
  if (!active_widget && !widgets_.empty()) {
    active_widget = widgets_[0].get();
  }

  for (const auto& widget : widgets_) {
    if (widget.get() == active_widget) {
      widget->Show();
    } else {
      widget->ShowInactive();
    }
  }

  if (active_widget) {
    active_widget->Activate();
    if (auto* contents = active_widget->GetContentsView()) {
      contents->RequestFocus();
    }
  }
}

std::unique_ptr<views::Widget>
OmniboxEverywhereRegionSelectOverlay::CreateWidgetForDisplay(
    const display::Display& display,
    const SkBitmap& display_bitmap,
    gfx::NativeWindow context) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "OmniboxEverywhereRegionSelectOverlay";
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
#if BUILDFLAG(IS_MAC)
  // Required to support Widget::SetActivationIndependence().
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
#else
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
#endif
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.visible_on_all_workspaces = true;
  params.bounds = display.bounds();
  if (context) {
    params.context = context;
  }

  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
#if BUILDFLAG(IS_MAC)
  widget->SetActivationIndependence(true);
  widget->SetCanAppearInExistingFullscreenSpaces(true);
#endif
  widget_observations_.AddObservation(widget.get());

#if defined(USE_AURA)
  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
                                             wm::ANIMATE_NONE);
#endif

  auto contents_view = std::make_unique<RegionSelectOverlayView>(
      display_bitmap,
      base::BindOnce(&OmniboxEverywhereRegionSelectOverlay::Finish,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&OmniboxEverywhereRegionSelectOverlay::Finish,
                     weak_factory_.GetWeakPtr(), SkBitmap()));
  widget->SetContentsView(std::move(contents_view));

  return widget;
}

void OmniboxEverywhereRegionSelectOverlay::Finish(
    const SkBitmap& result_bitmap) {
  if (!callback_) {
    return;
  }
  auto callback = std::move(callback_);

  widget_observations_.RemoveAllObservations();

  for (auto& widget : widgets_) {
    if (widget && !widget->IsClosed()) {
      widget->Hide();
      widget->Close();
    }
  }

  std::move(callback).Run(result_bitmap);
}

// Closing or destroying any widget (via Escape, OS dismissal, or display
// disconnect) cancels the entire selection overlay in unison.
// TODO(crbug.com/532198850): If a display is unplugged mid-selection, aborting
// capture is desirable since the desktop screenshot geometry is now stale.
void OmniboxEverywhereRegionSelectOverlay::OnWidgetClosing(
    views::Widget* widget) {
  Finish(SkBitmap());
}

void OmniboxEverywhereRegionSelectOverlay::OnWidgetDestroying(
    views::Widget* widget) {
  Finish(SkBitmap());
}

}  // namespace omnibox_everywhere
