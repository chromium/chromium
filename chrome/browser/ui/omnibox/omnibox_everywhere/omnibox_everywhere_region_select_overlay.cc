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
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

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

gfx::Rect GetOverlayBoundsForSource(const RegionCaptureSource& source) {
  auto* screen = display::Screen::Get();
  if (!screen) {
    return gfx::Rect();
  }

  // Single display (e.g. macOS 14+ native ScreenCaptureKit picker).
  if (source.type == RegionCaptureSource::Type::kSpecificDisplay &&
      source.display_id) {
    display::Display display;
    if (screen->GetDisplayWithDisplayId(*source.display_id, &display)) {
      return display.bounds();
    }
    // Fall back to primary display if specified display was disconnected.
    return screen->GetPrimaryDisplay().bounds();
  }

  // Full virtual desktop (Windows / Linux full desktop capture).
  if (source.type == RegionCaptureSource::Type::kAllDisplays) {
    gfx::Rect total_dip_bounds;
    for (const auto& display : screen->GetAllDisplays()) {
      total_dip_bounds.Union(display.bounds());
    }
    return total_dip_bounds;
  }

  return screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint())
      .bounds();
}

class RegionSelectOverlayView : public views::View {
  METADATA_HEADER(RegionSelectOverlayView, views::View)

 public:
  using ConfirmCallback = base::OnceCallback<void(const SkBitmap& cropped)>;

  RegionSelectOverlayView(const SkBitmap& screenshot,
                          const RegionCaptureSource& source,
                          ConfirmCallback on_confirm,
                          base::OnceClosure on_cancel)
      : bitmap_(screenshot),
        source_(source),
        on_confirm_(std::move(on_confirm)),
        on_cancel_(std::move(on_cancel)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    GetViewAccessibility().SetRole(ax::mojom::Role::kImage);
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_OMNIBOX_EVERYWHERE_REGION_SELECT_ACCESSIBLE_NAME));
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  }

  RegionSelectOverlayView(const RegionSelectOverlayView&) = delete;
  RegionSelectOverlayView& operator=(const RegionSelectOverlayView&) = delete;
  ~RegionSelectOverlayView() override = default;

  void DrawScreenshotImage(gfx::Canvas* canvas) {
    CHECK(!bitmap_.empty());
    gfx::ImageSkiaRep rep(bitmap_, canvas->image_scale());
    gfx::ImageSkia image(rep);
    canvas->DrawImageInt(image, 0, 0);
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::Cursor(ui::mojom::CursorType::kCross);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    if (bitmap_.empty() || width() <= 0 || height() <= 0) {
      canvas->DrawColor(SK_ColorBLACK);
      return;
    }

    // Draw base un-dimmed screenshot.
    DrawScreenshotImage(canvas);

    // Apply dark scrim over the screen.
    canvas->FillRect(GetLocalBounds(), kChromnientSlateScrim);

    if (selection_rect_.IsEmpty()) {
      // Draw Lens instruction chip when idle / pre-drag.
      DrawInstructionChip(canvas);
    } else {
      // Re-draw full-clarity screenshot inside selection_rect_.
      canvas->Save();
      canvas->ClipRect(selection_rect_);
      DrawScreenshotImage(canvas);
      canvas->Restore();

      // Draw corner reticles.
      DrawCornerReticles(canvas);
    }
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton()) {
      is_dragging_ = true;
      drag_start_ = event.location();
      selection_rect_ = gfx::Rect(drag_start_, gfx::Size(0, 0));
      SchedulePaint();
      return true;
    }
    return views::View::OnMousePressed(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (is_dragging_) {
      selection_rect_ = gfx::BoundingRect(drag_start_, event.location());
      SchedulePaint();
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
        SchedulePaint();
        event->SetHandled();
        break;
      case ui::EventType::kGestureScrollUpdate:
        if (is_dragging_) {
          selection_rect_ = gfx::BoundingRect(drag_start_, event->location());
          SchedulePaint();
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
  void DrawInstructionChip(gfx::Canvas* canvas) {
    const std::u16string toast_text = l10n_util::GetStringUTF16(
        IDS_LENS_OVERLAY_INITIAL_TOAST_MESSAGE_SIMPLIFIED);
    gfx::FontList font_list;
    const int text_width = gfx::Canvas::GetStringWidth(toast_text, font_list);
    constexpr int kHorizontalPadding = 20;
    const int chip_w = text_width + (kHorizontalPadding * 2);
    constexpr int chip_h = 40;
    const int chip_x = (width() - chip_w) / 2;
    constexpr int chip_y = 28;

    gfx::RectF chip_rect(chip_x, chip_y, chip_w, chip_h);

    // Pill background
    cc::PaintFlags fill_flags;
    fill_flags.setColor(SkColorSetA(SkColorSetRGB(0x18, 0x1C, 0x22), 220));
    fill_flags.setStyle(cc::PaintFlags::kFill_Style);
    fill_flags.setAntiAlias(true);
    canvas->DrawRoundRect(chip_rect, chip_h * 0.5f, fill_flags);

    // Border
    cc::PaintFlags border_flags;
    border_flags.setStyle(cc::PaintFlags::kStroke_Style);
    border_flags.setStrokeWidth(1.0f);
    border_flags.setColor(SkColorSetA(SK_ColorWHITE, 40));
    border_flags.setAntiAlias(true);
    canvas->DrawRoundRect(chip_rect, chip_h * 0.5f, border_flags);

    // Centered instruction text
    canvas->DrawStringRectWithFlags(toast_text, font_list,
                                    SkColorSetRGB(0xEE, 0xF0, 0xF9),
                                    gfx::Rect(chip_x, chip_y, chip_w, chip_h),
                                    gfx::Canvas::TEXT_ALIGN_CENTER);
  }

  void DrawCornerReticles(gfx::Canvas* canvas) {
    constexpr int kCornerLen = 20;
    const int max_corner_len =
        std::min(selection_rect_.width(), selection_rect_.height()) / 2;
    const int corner_len = std::min(kCornerLen, max_corner_len);

    cc::PaintFlags flags;
    flags.setColor(SK_ColorWHITE);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(3.5f);
    flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
    flags.setAntiAlias(true);

    for (int x : {selection_rect_.x(), selection_rect_.right()}) {
      for (int y : {selection_rect_.y(), selection_rect_.bottom()}) {
        int dx = (x == selection_rect_.x()) ? corner_len : -corner_len;
        int dy = (y == selection_rect_.y()) ? corner_len : -corner_len;
        canvas->DrawLine(gfx::PointF(x, y), gfx::PointF(x + dx, y), flags);
        canvas->DrawLine(gfx::PointF(x, y), gfx::PointF(x, y + dy), flags);
      }
    }
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
  RegionCaptureSource source_;
  ConfirmCallback on_confirm_;
  base::OnceClosure on_cancel_;
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
  widget_observation_.Reset();
  if (widget_) {
    widget_->CloseNow();
    widget_.reset();
  }
  if (callback_) {
    std::move(callback_).Run(SkBitmap());
  }
}

void OmniboxEverywhereRegionSelectOverlay::Initialize(
    const SkBitmap& screenshot,
    const RegionCaptureSource& source,
    gfx::NativeWindow context) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "OmniboxEverywhereRegionSelectOverlay";
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.bounds = GetOverlayBoundsForSource(source);
  if (context) {
    params.context = context;
  }

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  widget_observation_.Observe(widget_.get());

#if defined(USE_AURA)
  wm::SetWindowVisibilityAnimationTransition(widget_->GetNativeView(),
                                             wm::ANIMATE_NONE);
#endif

  auto contents_view = std::make_unique<RegionSelectOverlayView>(
      screenshot, source,
      base::BindOnce(&OmniboxEverywhereRegionSelectOverlay::Finish,
                     base::Unretained(this)),
      base::BindOnce(&OmniboxEverywhereRegionSelectOverlay::Finish,
                     base::Unretained(this), SkBitmap()));
  views::View* view = widget_->SetContentsView(std::move(contents_view));
  widget_->Show();
  widget_->Activate();
  view->RequestFocus();
}

void OmniboxEverywhereRegionSelectOverlay::Finish(
    const SkBitmap& result_bitmap) {
  auto callback = std::move(callback_);
  if (widget_) {
    widget_observation_.Reset();
    widget_->Hide();
  }
  if (callback) {
    std::move(callback).Run(result_bitmap);
  }
}

void OmniboxEverywhereRegionSelectOverlay::OnWidgetClosing(
    views::Widget* widget) {
  if (callback_) {
    std::move(callback_).Run(SkBitmap());
  }
}

void OmniboxEverywhereRegionSelectOverlay::OnWidgetDestroying(
    views::Widget* widget) {
  widget_observation_.Reset();
  if (callback_) {
    std::move(callback_).Run(SkBitmap());
  }
}

}  // namespace omnibox_everywhere
