// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_region_select_overlay.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
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

gfx::Rect GetOverlayBoundsForSource(const RegionCaptureSource& source) {
  auto* screen = display::Screen::Get();
  if (!screen) {
    return gfx::Rect();
  }

  // Single display (e.g. macOS 14+ native ScreenCaptureKit picker)
  if (source.type == RegionCaptureSource::Type::kSpecificDisplay &&
      source.display_id) {
    display::Display display;
    if (screen->GetDisplayWithDisplayId(*source.display_id, &display)) {
      return display.bounds();
    }
  }

  // Full virtual desktop (Windows / Linux full desktop capture)
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
  RegionSelectOverlayView(const SkBitmap& screenshot,
                          const RegionCaptureSource& source,
                          base::OnceClosure on_confirm,
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

  void OnPaint(gfx::Canvas* canvas) override {
    if (bitmap_.empty() || width() <= 0 || height() <= 0) {
      canvas->DrawColor(SK_ColorBLACK);
      return;
    }

    canvas->DrawColor(SK_ColorBLACK);
    DrawScreenshotImage(canvas);
  }

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    if (accelerator.key_code() == ui::VKEY_ESCAPE) {
      Cancel();
      return true;
    }
    return false;
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton() && on_confirm_) {
      std::move(on_confirm_).Run();
      return true;
    }
    return views::View::OnMousePressed(event);
  }

 private:
  void Cancel() {
    if (on_cancel_) {
      std::move(on_cancel_).Run();
    }
  }

  SkBitmap bitmap_;
  RegionCaptureSource source_;
  base::OnceClosure on_confirm_;
  base::OnceClosure on_cancel_;
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
                     base::Unretained(this), screenshot),
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
