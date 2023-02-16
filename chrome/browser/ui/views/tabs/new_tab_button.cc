// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/new_tab_button.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/generated_resources.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

// static
const gfx::Size NewTabButton::kButtonSize{28, 28};

class NewTabButton::HighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  HighlightPathGenerator() = default;
  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return static_cast<const NewTabButton*>(view)->GetBorderPath(
        view->GetContentsBounds().origin(), 1.0f, false);
  }
};

NewTabButton::NewTabButton(TabStrip* tab_strip, PressedCallback callback)
    : views::ImageButton(std::move(callback)), tab_strip_(tab_strip) {
  SetAnimateOnStateChange(true);
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  SetTriggerableEventFlags(GetTriggerableEventFlags() |
                           ui::EF_MIDDLE_MOUSE_BUTTON);
#endif

  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetHighlightOpacity(0.16f);
  views::InkDrop::Get(this)->SetVisibleOpacity(0.14f);

  SetInstallFocusRingOnFocus(true);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<NewTabButton::HighlightPathGenerator>());

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  SetProperty(views::kElementIdentifierKey, kNewTabButtonElementId);
}

NewTabButton::~NewTabButton() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/RemoveLayerFromRegions(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

void NewTabButton::FrameColorsChanged() {
  const auto* const color_provider = GetColorProvider();
  views::FocusRing::Get(this)->SetColorId(kColorNewTabButtonFocusRing);
  views::InkDrop::Get(this)->SetBaseColor(
      color_provider->GetColor(tab_strip_->ShouldPaintAsActiveFrame()
                                   ? kColorNewTabButtonInkDropFrameActive
                                   : kColorNewTabButtonInkDropFrameInactive));
  SchedulePaint();
}

void NewTabButton::AnimateToStateForTesting(views::InkDropState state) {
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(state);
}

void NewTabButton::AddLayerToRegion(ui::Layer* new_layer,
                                    views::LayerRegion region) {
  ink_drop_container_->AddLayerToRegion(new_layer, region);
}

void NewTabButton::RemoveLayerFromRegions(ui::Layer* old_layer) {
  ink_drop_container_->RemoveLayerFromRegions(old_layer);
}

SkColor NewTabButton::GetForegroundColor() const {
  return tab_strip_->GetTabForegroundColor(TabActive::kInactive);
}

void NewTabButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ImageButton::OnBoundsChanged(previous_bounds);
  ink_drop_container_->SetBoundsRect(GetLocalBounds());
}

void NewTabButton::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &NewTabButton::FrameColorsChanged, base::Unretained(this)));
}

void NewTabButton::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

void NewTabButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  FrameColorsChanged();
}

#if BUILDFLAG(IS_WIN)
void NewTabButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (!event.IsOnlyRightMouseButton()) {
    views::ImageButton::OnMouseReleased(event);
    return;
  }

  // TODO(pkasting): If we handled right-clicks on the frame, and we made sure
  // this event was not handled, it seems like things would Just Work.
  gfx::Point point = event.location();
  views::View::ConvertPointToScreen(this, &point);
  point = display::win::ScreenWin::DIPToScreenPoint(point);
  auto weak_this = weak_factory_.GetWeakPtr();
  views::ShowSystemMenuAtScreenPixelLocation(views::HWNDForView(this), point);
  if (!weak_this)
    return;
  SetState(views::Button::STATE_NORMAL);
}
#endif

void NewTabButton::OnGestureEvent(ui::GestureEvent* event) {
  // Consume all gesture events here so that the parent (Tab) does not
  // start consuming gestures.
  views::ImageButton::OnGestureEvent(event);
  event->SetHandled();
}

void NewTabButton::NotifyClick(const ui::Event& event) {
  ImageButton::NotifyClick(event);
  views::InkDrop::Get(this)->GetInkDrop()->AnimateToState(
      views::InkDropState::ACTION_TRIGGERED);
}

void NewTabButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->Translate(GetContentsBounds().OffsetFromOrigin());
  PaintFill(canvas);
  PaintIcon(canvas);
}

gfx::Size NewTabButton::CalculatePreferredSize() const {
  gfx::Size size = kButtonSize;
  const auto insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

bool NewTabButton::GetHitTestMask(SkPath* mask) const {
  DCHECK(mask);

  gfx::Point origin = GetContentsBounds().origin();
  if (base::i18n::IsRTL())
    origin.set_x(GetInsets().right());
  const float scale = GetWidget()->GetCompositor()->device_scale_factor();
  SkPath border = GetBorderPath(origin, scale,
                                tab_strip_->controller()->IsFrameCondensed());
  mask->addPath(border, SkMatrix::Scale(1 / scale, 1 / scale));
  return true;
}

int NewTabButton::GetCornerRadius() const {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, GetContentsBounds().size());
}

void NewTabButton::PaintFill(gfx::Canvas* canvas) const {
  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->UndoDeviceScaleFactor();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  const float scale = canvas->image_scale();
  const absl::optional<int> bg_id =
      tab_strip_->GetCustomBackgroundId(BrowserFrameActiveState::kUseCurrent);
  if (bg_id.has_value()) {
    float x_scale = scale;
    const gfx::Rect& contents_bounds = GetContentsBounds();
    gfx::RectF bounds_in_tab_strip(GetLocalBounds());
    View::ConvertRectToTarget(this, tab_strip_, &bounds_in_tab_strip);
    int x = bounds_in_tab_strip.x() + contents_bounds.x() +
            tab_strip_->GetBackgroundOffset();
    if (base::i18n::IsRTL()) {
      // The new tab background is mirrored in RTL mode, but the theme
      // background should never be mirrored. Mirror it here to compensate.
      x_scale = -scale;
      // Offset by |width| such that the same region is painted as if there
      // was no flip.
      x += contents_bounds.width();
    }

    canvas->InitPaintFlagsForTiling(
        *GetThemeProvider()->GetImageSkiaNamed(bg_id.value()), x,
        contents_bounds.y(), x_scale, scale, 0, 0, SkTileMode::kRepeat,
        SkTileMode::kRepeat, &flags);
  } else {
    flags.setColor(GetColorProvider()->GetColor(
        tab_strip_->ShouldPaintAsActiveFrame()
            ? kColorNewTabButtonBackgroundFrameActive
            : kColorNewTabButtonBackgroundFrameInactive));
  }

  canvas->DrawPath(GetBorderPath(gfx::Point(), scale, false), flags);
}

void NewTabButton::PaintIcon(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetForegroundColor());
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  constexpr int kStrokeWidth = 2;
  flags.setStrokeWidth(kStrokeWidth);

  const int radius = ui::TouchUiController::Get()->touch_ui() ? 7 : 6;
  const int offset = GetCornerRadius() - radius;
  // The cap will be added outside the end of the stroke; inset to compensate.
  constexpr int kCapRadius = kStrokeWidth / 2;
  const int start = offset + kCapRadius;
  const int end = offset + (radius * 2) - kCapRadius;
  const int center = offset + radius;

  // Horizontal stroke.
  canvas->DrawLine(gfx::PointF(start, center), gfx::PointF(end, center), flags);

  // Vertical stroke.
  canvas->DrawLine(gfx::PointF(center, start), gfx::PointF(center, end), flags);
}

SkPath NewTabButton::GetBorderPath(const gfx::Point& origin,
                                   float scale,
                                   bool extend_to_top) const {
  gfx::PointF scaled_origin(origin);
  scaled_origin.Scale(scale);
  const float radius = GetCornerRadius() * scale;

  SkPath path;
  if (extend_to_top) {
    path.moveTo(scaled_origin.x(), 0);
    const float diameter = radius * 2;
    path.rLineTo(diameter, 0);
    path.rLineTo(0, scaled_origin.y() + radius);
    path.rArcTo(radius, radius, 0, SkPath::kSmall_ArcSize, SkPathDirection::kCW,
                -diameter, 0);
    path.close();
  } else {
    path.addCircle(scaled_origin.x() + radius, scaled_origin.y() + radius,
                   radius);
  }
  return path;
}

BEGIN_METADATA(NewTabButton, views::ImageButton)
END_METADATA
