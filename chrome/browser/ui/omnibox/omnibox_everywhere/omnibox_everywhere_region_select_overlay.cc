// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_region_select_overlay.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
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
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
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
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#endif

#if defined(USE_AURA)
#include "ui/wm/core/window_animations.h"
#endif

namespace omnibox_everywhere {

namespace {

using RegionCaptureSource =
    OmniboxEverywhereRegionSelectOverlay::RegionCaptureSource;

// Lens dark slate base color and derived alphas.
constexpr SkColor kSlateBaseColor = SkColorSetRGB(0x18, 0x1C, 0x22);
constexpr SkColor kChromnientSlateScrim = SkColorSetA(kSlateBaseColor, 165);
constexpr SkColor kToastBackgroundColor = SkColorSetA(kSlateBaseColor, 220);

constexpr float kSelectionRectStrokeWidth = 2.5f;
constexpr int kSelectionStrokeOutset = 4;
constexpr int kSelectionCornerRadius = 14;

constexpr float kGlifGradientWashAlpha = 0.18f;

constexpr int kMinSelectionSize = 10;

struct IntersectingSlice {
  gfx::Rect dip_intersection;
  gfx::Rect phys_rect_in_screenshot;
  float scale = 1.0f;
};

gfx::Rect MapDipToPhysicalBounds(const gfx::Rect& dip_rect,
                                 const gfx::Rect& display_bounds,
                                 const gfx::Rect& pixel_bounds) {
  if (display_bounds.IsEmpty()) {
    return gfx::Rect();
  }
  const float scale_x =
      static_cast<float>(pixel_bounds.width()) / display_bounds.width();
  const float scale_y =
      static_cast<float>(pixel_bounds.height()) / display_bounds.height();
  return gfx::ScaleToRoundedRect(dip_rect - display_bounds.OffsetFromOrigin(),
                                 scale_x, scale_y) +
         pixel_bounds.OffsetFromOrigin();
}

SkColor4f ColorWithAlpha(SkColor color, float alpha) {
  SkColor4f c = SkColor4f::FromColor(color);
  c.fA = alpha;
  return c;
}

// Extracts a cropped subset of `source` into a newly allocated buffer.
// This ensures row size matches the cropped width (required for Mojo
// serialization), while releasing the full-desktop SkPixelRef from memory.
SkBitmap ExtractCompactSubset(const SkBitmap& source,
                              const gfx::Rect& crop_rect) {
  if (source.drawsNothing() || crop_rect.IsEmpty()) {
    return SkBitmap();
  }
  gfx::Rect safe_crop = crop_rect;
  safe_crop.Intersect(gfx::Rect(source.width(), source.height()));
  if (safe_crop.IsEmpty()) {
    return SkBitmap();
  }
  // Allocating independent pixels and reading straight from the source offset
  // detaches the result from the source's SkPixelRef without an intermediate
  // subset bitmap.
  SkBitmap compact_copy;
  if (!compact_copy.tryAllocPixels(
          source.info().makeWH(safe_crop.width(), safe_crop.height()))) {
    return SkBitmap();
  }
  if (!source.readPixels(compact_copy.pixmap(), safe_crop.x(), safe_crop.y())) {
    return SkBitmap();
  }
  compact_copy.setImmutable();
  return compact_copy;
}

std::pair<gfx::Rect, float> CalculateActiveBoundsAndScale(
    base::span<const IntersectingSlice> slices) {
  gfx::Rect active_dip_bounds;
  float output_scale = 0.0f;
  int64_t dominant_area = 0;

  for (const auto& slice : slices) {
    active_dip_bounds.Union(slice.dip_intersection);
    const int64_t area = static_cast<int64_t>(slice.dip_intersection.width()) *
                         slice.dip_intersection.height();
    if (area > dominant_area ||
        (area == dominant_area && slice.scale > output_scale)) {
      dominant_area = area;
      output_scale = slice.scale;
    }
  }
  return {active_dip_bounds, output_scale};
}

void DrawSlicesToCanvas(const SkBitmap& screenshot,
                        base::span<const IntersectingSlice> slices,
                        const gfx::Rect& active_dip_bounds,
                        float output_scale,
                        SkBitmap& output) {
  // If displays aren't perfectly aligned, then the "nothing" space will be
  // black.
  output.eraseColor(SK_ColorBLACK);

  SkCanvas canvas(output);
  SkPaint sk_paint;
  sk_paint.setBlendMode(SkBlendMode::kSrc);

  for (const auto& item : slices) {
    SkBitmap piece;
    if (!screenshot.extractSubset(
            &piece, gfx::RectToSkIRect(item.phys_rect_in_screenshot))) {
      continue;
    }

    const gfx::Rect dest_rect = gfx::ScaleToRoundedRect(
        item.dip_intersection - active_dip_bounds.OffsetFromOrigin(),
        output_scale);
    if (dest_rect.IsEmpty()) {
      continue;
    }

    const bool needs_resample = dest_rect.width() != piece.width() ||
                                dest_rect.height() != piece.height();
    const SkSamplingOptions sampling =
        needs_resample
            ? SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone)
            : SkSamplingOptions();

    canvas.drawImageRect(piece.asImage(),
                         SkRect::MakeIWH(piece.width(), piece.height()),
                         gfx::RectToSkRect(dest_rect), sampling, &sk_paint,
                         SkCanvas::kStrict_SrcRectConstraint);
  }
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
    fill_flags.setColor(kToastBackgroundColor);
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
    constexpr int kIconSize = 20;
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

}  // namespace

class RegionSelectOverlayView : public views::View {
  METADATA_HEADER(RegionSelectOverlayView, views::View)

 public:
  RegionSelectOverlayView(OmniboxEverywhereRegionSelectOverlay& coordinator,
                          const SkBitmap& screenshot,
                          const display::Display& display)
      : coordinator_(coordinator),
        display_(display),
#if BUILDFLAG(IS_WIN)
        display_physical_offset_(
            GetDisplayPhysicalBounds(display).OffsetFromOrigin()),
#endif
        bitmap_(screenshot),
        image_(!bitmap_.empty() ? gfx::ImageSkia::CreateFromBitmap(bitmap_, 1.f)
                                : gfx::ImageSkia()) {
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

  // Returns `event`'s location in the virtual desktop's DIP coordinate space,
  // i.e. the same space as `display::Display::bounds()`.
  //
  // A mouse drag holds pointer capture and can therefore travel onto a monitor
  // other than the one hosting this widget. So simply offsetting it by this
  // widget's display origin would drift further from true cursor position.
  gfx::Point GetScreenPointForEvent(const ui::LocatedEvent& event) const {
#if BUILDFLAG(IS_WIN)
    if (!GetLocalBounds().Contains(event.location())) {
      if (const auto* screen_win = display::win::GetScreenWin();
          screen_win && display::Screen::Get() == screen_win) {
        const gfx::PointF phys_pt =
            gfx::ScalePoint(event.location_f(),
                            display_.device_scale_factor()) +
            display_physical_offset_;
        return gfx::ToRoundedPoint(screen_win->ScreenToDIPPoint(phys_pt));
      }
    }
#endif
    return display_.bounds().origin() + event.location().OffsetFromOrigin();
  }

  gfx::Rect GetGlobalSelectionRect(const ui::LocatedEvent& event) const {
    return gfx::BoundingRect(drag_start_screen_, GetScreenPointForEvent(event));
  }

  void HideToastChip() {
    if (toast_chip_) {
      toast_chip_->SetVisible(false);
    }
  }

  void ClearBitmaps() {
    is_dragging_ = false;
    bitmap_.reset();
    image_ = gfx::ImageSkia();
  }

  void SetGlobalSelectionRect(const gfx::Rect& global_rect) {
    gfx::Rect new_rect;
    if (!global_rect.IsEmpty()) {
      new_rect = global_rect;
      new_rect.Offset(-display_.bounds().OffsetFromOrigin());
      // Cull selections that do not intersect this display widget's bounds to
      // prevent scheduling invalidations on non-intersecting monitors. The
      // outset keeps the sliver of border that spills across a monitor seam.
      gfx::Rect visible_bounds = GetLocalBounds();
      visible_bounds.Outset(kSelectionStrokeOutset);
      if (!new_rect.Intersects(visible_bounds)) {
        new_rect = gfx::Rect();
      }
    }
    UpdateSelectionRect(new_rect);
  }

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

    const bool has_selection = !selection_rect_.IsEmpty();

    if (has_selection) {
      canvas->Save();
      // Clip out the selection so the scrim and rainbow wash are applied only
      // to unselected regions.
      ClipSelection(canvas);
    }

    // 2. Apply dark scrim over the unselected area.
    canvas->FillRect(GetLocalBounds(), kChromnientSlateScrim);

    // 3. GLIF rainbow gradient wash over the unselected area.
    DrawRainbowGradientWash(canvas);

    if (has_selection) {
      canvas->Restore();

      // 4. Perimeter border with rounded corners.
      DrawSelectionBorder(canvas);
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
      drag_start_screen_ = GetScreenPointForEvent(event);
      if (cursor_chip_) {
        cursor_chip_->SetVisible(false);
      }
      coordinator_->OnDragStarted();
      coordinator_->OnDragUpdated(
          gfx::Rect(drag_start_screen_, gfx::Size(0, 0)));
      return true;
    }
    return views::View::OnMousePressed(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (is_dragging_) {
      coordinator_->OnDragUpdated(GetGlobalSelectionRect(event));
      return true;
    }
    return views::View::OnMouseDragged(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (is_dragging_ && event.IsLeftMouseButton()) {
      is_dragging_ = false;
      coordinator_->OnDragCompleted(GetGlobalSelectionRect(event));
    }
  }

  void OnMouseCaptureLost() override {
    if (is_dragging_) {
      is_dragging_ = false;
      coordinator_->OnDragCancelled();
    }
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    switch (event->type()) {
      case ui::EventType::kGestureTapDown:
        event->SetHandled();
        break;
      case ui::EventType::kGestureTapCancel:
        if (is_dragging_) {
          is_dragging_ = false;
          coordinator_->OnDragCancelled();
        }
        event->SetHandled();
        break;
      case ui::EventType::kGestureScrollBegin:
        is_dragging_ = true;
        drag_start_screen_ = GetScreenPointForEvent(*event);
        if (cursor_chip_) {
          cursor_chip_->SetVisible(false);
        }
        coordinator_->OnDragStarted();
        coordinator_->OnDragUpdated(
            gfx::Rect(drag_start_screen_, gfx::Size(0, 0)));
        event->SetHandled();
        break;
      case ui::EventType::kGestureScrollUpdate:
        if (is_dragging_) {
          coordinator_->OnDragUpdated(GetGlobalSelectionRect(*event));
          event->SetHandled();
        }
        break;
      case ui::EventType::kGestureScrollEnd:
      case ui::EventType::kGestureEnd:
        if (is_dragging_) {
          is_dragging_ = false;
          coordinator_->OnDragCompleted(GetGlobalSelectionRect(*event));
          event->SetHandled();
        }
        break;
      default:
        break;
    }
  }

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    if (accelerator.key_code() == ui::VKEY_ESCAPE) {
      is_dragging_ = false;
      coordinator_->OnDragCancelled();
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
    const int toast_x = std::max(0, (width() - toast_size.width()) / 2);
    const int toast_y = kTopMargin;
    toast_chip_->SetBounds(toast_x, toast_y, toast_size.width(),
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

    constexpr int kVisualOffset = 10;
    constexpr int kOffset = kVisualOffset - TeardropCursorChipView::kPadding;
    cursor_chip_->SetBounds(pos.x() + kOffset, pos.y() + kOffset,
                            TeardropCursorChipView::kTotalSize,
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

  SkPath GetSelectionPath() const {
    if (selection_rect_.IsEmpty()) {
      return SkPath();
    }
    const float corner_radius = std::min({
        static_cast<float>(kSelectionCornerRadius),
        selection_rect_.width() / 2.0f,
        selection_rect_.height() / 2.0f,
    });

    SkRect sk_sel_rect =
        SkRect::MakeXYWH(selection_rect_.x(), selection_rect_.y(),
                         selection_rect_.width(), selection_rect_.height());
    return SkPath::RRect(
        SkRRect::MakeRectXY(sk_sel_rect, corner_radius, corner_radius));
  }

  void ClipSelection(gfx::Canvas* canvas) {
    if (selection_rect_.IsEmpty()) {
      return;
    }
    // Clip out the selection so the scrim and rainbow wash are applied only
    // to unselected regions.
    canvas->ClipPath(GetSelectionPath(), /*do_anti_alias=*/true,
                     SkClipOp::kDifference);
  }

  void DrawSelectionBorder(gfx::Canvas* canvas) {
    if (selection_rect_.IsEmpty()) {
      return;
    }
    // Perimeter border with rounded corners.
    cc::PaintFlags stroke_flags;
    stroke_flags.setColor(SK_ColorWHITE);
    stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
    stroke_flags.setStrokeWidth(kSelectionRectStrokeWidth);
    stroke_flags.setAntiAlias(true);
    canvas->DrawPath(GetSelectionPath(), stroke_flags);
  }

  void UpdateSelectionRect(const gfx::Rect& new_rect) {
    if (new_rect == selection_rect_) {
      return;
    }
    gfx::Rect damage_rect = gfx::UnionRects(selection_rect_, new_rect);
    // Expand by stroke width plus anti-aliasing margin so the perimeter
    // border is completely cleared and redrawn.
    damage_rect.Outset(kSelectionStrokeOutset);
    // The selection can extend past this monitor; only invalidate what is
    // actually painted.
    damage_rect.Intersect(GetLocalBounds());
    selection_rect_ = new_rect;
    SchedulePaintInRect(damage_rect);
  }

  const raw_ref<OmniboxEverywhereRegionSelectOverlay> coordinator_;
  const display::Display display_;
#if BUILDFLAG(IS_WIN)
  const gfx::Vector2d display_physical_offset_;
#endif
  SkBitmap bitmap_;
  gfx::ImageSkia image_;
  raw_ptr<InstructionToastChipView> toast_chip_ = nullptr;
  raw_ptr<TeardropCursorChipView> cursor_chip_ = nullptr;
  gfx::Point drag_start_screen_;
  gfx::Rect selection_rect_;
  bool is_dragging_ = false;
};

BEGIN_METADATA(RegionSelectOverlayView)
END_METADATA

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
  auto callback = std::move(callback_);
  widget_observations_.RemoveAllObservations();
  for (auto& widget : widgets_) {
    if (widget) {
      if (auto* overlay_view = views::AsViewClass<RegionSelectOverlayView>(
              widget->GetContentsView())) {
        overlay_view->ClearBitmaps();
      }
      // Not reachable from a Widget close notification, so the widget can be
      // torn down synchronously here.
      if (!widget->IsClosed()) {
        widget->CloseNow();
      }
    }
  }
  widgets_.clear();
  screenshot_.reset();
  display_slices_.clear();
  if (callback) {
    std::move(callback).Run(SkBitmap());
  }
}

size_t OmniboxEverywhereRegionSelectOverlay::GetActiveWidgetIndex() const {
  if (widgets_.empty()) {
    return 0;
  }
  auto* screen = display::Screen::Get();
  if (screen) {
    const gfx::Point cursor = screen->GetCursorScreenPoint();
    // Display bounds are the source of truth for which monitor owns the
    // cursor, and are unaffected by any window-manager adjustment of the
    // widget frame.
    for (size_t i = 0; i < display_slices_.size() && i < widgets_.size(); ++i) {
      if (display_slices_[i].display.bounds().Contains(cursor)) {
        return i;
      }
    }
    // `display_slices_` is cleared by Finish(), so fall back to the widget
    // bounds once the overlay has completed.
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
  if (target_displays.empty() || screenshot.drawsNothing()) {
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

  screenshot_ = screenshot;
  screenshot_.setImmutable();
  display_slices_.clear();

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
    display_slices_.push_back({display, sub_rect});

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

  auto contents_view =
      std::make_unique<RegionSelectOverlayView>(*this, display_bitmap, display);
  widget->SetContentsView(std::move(contents_view));

  return widget;
}

void OmniboxEverywhereRegionSelectOverlay::OnDragStarted() {
  for (const auto& widget : widgets_) {
    if (widget) {
      if (auto* overlay_view = views::AsViewClass<RegionSelectOverlayView>(
              widget->GetContentsView())) {
        overlay_view->HideToastChip();
      }
    }
  }
}

void OmniboxEverywhereRegionSelectOverlay::OnDragUpdated(
    const gfx::Rect& global_selection_rect) {
  for (const auto& widget : widgets_) {
    if (widget) {
      if (auto* overlay_view = views::AsViewClass<RegionSelectOverlayView>(
              widget->GetContentsView())) {
        overlay_view->SetGlobalSelectionRect(global_selection_rect);
      }
    }
  }
}

void OmniboxEverywhereRegionSelectOverlay::OnDragCompleted(
    const gfx::Rect& global_selection_rect) {
  Finish(CropGlobalSelection(global_selection_rect));
}

void OmniboxEverywhereRegionSelectOverlay::OnDragCancelled() {
  Finish(SkBitmap());
}

SkBitmap OmniboxEverywhereRegionSelectOverlay::CropGlobalSelection(
    const gfx::Rect& global_selection_rect) const {
  if (screenshot_.drawsNothing() || global_selection_rect.IsEmpty()) {
    return SkBitmap();
  }

  std::vector<IntersectingSlice> intersecting;
  const gfx::Rect screenshot_bounds = gfx::SkIRectToRect(screenshot_.bounds());

  for (const auto& slice : display_slices_) {
    // Find the intersection of the selection with the display.
    // Skip if no intersection.
    // Otherwise, map into physical bounds and union with the rest of the
    // selection.
    const gfx::Rect inter_dip =
        gfx::IntersectRects(global_selection_rect, slice.display.bounds());
    if (inter_dip.IsEmpty()) {
      continue;
    }

    gfx::Rect phys_crop = MapDipToPhysicalBounds(
        inter_dip, slice.display.bounds(), slice.sub_rect_in_screenshot);
    phys_crop.Intersect(slice.sub_rect_in_screenshot);
    phys_crop.Intersect(screenshot_bounds);
    if (phys_crop.IsEmpty()) {
      continue;
    }

    const float slice_scale =
        std::max(static_cast<float>(slice.sub_rect_in_screenshot.width()) /
                     slice.display.bounds().width(),
                 static_cast<float>(slice.sub_rect_in_screenshot.height()) /
                     slice.display.bounds().height());

    intersecting.push_back({inter_dip, phys_crop, slice_scale});
  }

  const auto [active_dip_bounds, output_scale] =
      CalculateActiveBoundsAndScale(intersecting);
  if (active_dip_bounds.width() < kMinSelectionSize ||
      active_dip_bounds.height() < kMinSelectionSize) {
    return SkBitmap();
  }

  if (intersecting.size() == 1) {
    return ExtractCompactSubset(screenshot_,
                                intersecting[0].phys_rect_in_screenshot);
  }

  const int out_width =
      std::max(1, base::ClampRound(active_dip_bounds.width() * output_scale));
  const int out_height =
      std::max(1, base::ClampRound(active_dip_bounds.height() * output_scale));

  SkBitmap output;
  if (!output.tryAllocPixels(
          screenshot_.info().makeWH(out_width, out_height))) {
    return SkBitmap();
  }

  DrawSlicesToCanvas(screenshot_, intersecting, active_dip_bounds, output_scale,
                     output);

  output.setImmutable();
  return output;
}

void OmniboxEverywhereRegionSelectOverlay::Finish(
    const SkBitmap& result_bitmap) {
  if (!callback_) {
    return;
  }
  auto callback = std::move(callback_);

  widget_observations_.RemoveAllObservations();

  for (auto& widget : widgets_) {
    if (widget) {
      if (auto* overlay_view = views::AsViewClass<RegionSelectOverlayView>(
              widget->GetContentsView())) {
        overlay_view->ClearBitmaps();
      }
      if (!widget->IsClosed()) {
        // Hide before closing so this fullscreen overlay skips any platform
        // close animation and prevents paint flashes.
        widget->Hide();
        // This may be running inside a Widget close notification, where the
        // Widget forbids synchronous destruction, so close asynchronously.
        widget->Close();
      }
    }
  }

  screenshot_.reset();
  display_slices_.clear();

  std::move(callback).Run(result_bitmap);
}

// Closing or destroying any widget cancels the selection overlay in unison.
void OmniboxEverywhereRegionSelectOverlay::OnWidgetClosing(
    views::Widget* widget) {
  Finish(SkBitmap());
}

void OmniboxEverywhereRegionSelectOverlay::OnWidgetDestroying(
    views::Widget* widget) {
  Finish(SkBitmap());
}

}  // namespace omnibox_everywhere
