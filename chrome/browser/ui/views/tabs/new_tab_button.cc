// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/new_tab_button.h"

#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/feature_promos/new_tab_promo_bubble_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/feature_engagement/buildflags.h"
#include "ui/base/default_theme_provider.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker_factory.h"
#include "chrome/browser/ui/views/feature_promos/new_tab_promo_bubble_view.h"
#endif

// static
const gfx::Size NewTabButton::kButtonSize{28, 28};

NewTabButton::NewTabButton(TabStrip* tab_strip, views::ButtonListener* listener)
    : views::ImageButton(listener), tab_strip_(tab_strip) {
  set_animate_on_state_change(true);
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  set_triggerable_event_flags(triggerable_event_flags() |
                              ui::EF_MIDDLE_MOUSE_BUTTON);
#endif

  // Initialize the ink drop mode for a ripple highlight on button press.
  ink_drop_container_ = new views::InkDropContainerView();
  AddChildView(ink_drop_container_);
  ink_drop_container_->SetVisible(false);
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  set_ink_drop_visible_opacity(0.08f);

  SetFocusPainter(nullptr);
  SetInstallFocusRingOnFocus(true);
}

NewTabButton::~NewTabButton() {
  if (destroyed_)
    *destroyed_ = true;
}

// static
void NewTabButton::ShowPromoForLastActiveBrowser() {
  BrowserView* browser = static_cast<BrowserView*>(
      BrowserList::GetInstance()->GetLastActive()->window());
  browser->tabstrip()->new_tab_button()->ShowPromo();
}

// static
void NewTabButton::CloseBubbleForLastActiveBrowser() {
  BrowserView* browser = static_cast<BrowserView*>(
      BrowserList::GetInstance()->GetLastActive()->window());
  browser->tabstrip()->new_tab_button()->CloseBubble();
}

void NewTabButton::ShowPromo() {
  DCHECK(!new_tab_promo_);
  // Owned by its native widget. Will be destroyed as its widget is destroyed.
  new_tab_promo_ = NewTabPromoBubbleView::CreateOwned(this);
  new_tab_promo_observer_.Add(new_tab_promo_->GetWidget());
  SchedulePaint();
}

void NewTabButton::CloseBubble() {
  if (new_tab_promo_)
    new_tab_promo_->CloseBubble();
}

void NewTabButton::FrameColorsChanged() {
  UpdateInkDropBaseColor();
}

void NewTabButton::AnimateInkDropToStateForTesting(views::InkDropState state) {
  GetInkDrop()->AnimateToState(state);
}

#if defined(OS_WIN)
void NewTabButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (!event.IsOnlyRightMouseButton()) {
    views::ImageButton::OnMouseReleased(event);
    return;
  }

  gfx::Point point = event.location();
  views::View::ConvertPointToScreen(this, &point);
  point = display::win::ScreenWin::DIPToScreenPoint(point);
  bool destroyed = false;
  destroyed_ = &destroyed;
  gfx::ShowSystemMenuAtPoint(views::HWNDForView(this), point);
  if (!destroyed)
    SetState(views::Button::STATE_NORMAL);
}
#endif

void NewTabButton::OnGestureEvent(ui::GestureEvent* event) {
  // Consume all gesture events here so that the parent (Tab) does not
  // start consuming gestures.
  views::ImageButton::OnGestureEvent(event);
  event->SetHandled();
}

void NewTabButton::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  DCHECK(ink_drop_layer->bounds().size() == GetContentsBounds().size());
  DCHECK(ink_drop_container_->bounds().size() == GetContentsBounds().size());
  ink_drop_container_->AddInkDropLayer(ink_drop_layer);
  InstallInkDropMask(ink_drop_layer);
}

void NewTabButton::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  ResetInkDropMask();
  ink_drop_container_->RemoveInkDropLayer(ink_drop_layer);
}

std::unique_ptr<views::InkDropRipple> NewTabButton::CreateInkDropRipple()
    const {
  const gfx::Rect contents_bounds = GetContentsBounds();
  return std::make_unique<views::FloodFillInkDropRipple>(
      contents_bounds.size(), gfx::Insets(),
      GetInkDropCenterBasedOnLastEvent() - contents_bounds.OffsetFromOrigin(),
      GetInkDropBaseColor(), ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropHighlight> NewTabButton::CreateInkDropHighlight()
    const {
  const gfx::Rect bounds(GetContentsBounds().size());
  auto highlight = CreateDefaultInkDropHighlight(
      gfx::RectF(bounds).CenterPoint(), bounds.size());
  highlight->set_visible_opacity(0.1f);
  return highlight;
}

void NewTabButton::NotifyClick(const ui::Event& event) {
  ImageButton::NotifyClick(event);
  GetInkDrop()->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
}

std::unique_ptr<views::InkDrop> NewTabButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      std::make_unique<views::InkDropImpl>(this, GetContentsBounds().size());
  ink_drop->SetAutoHighlightMode(views::InkDropImpl::AutoHighlightMode::NONE);
  ink_drop->SetShowHighlightOnHover(true);
  UpdateInkDropBaseColor();
  return ink_drop;
}

std::unique_ptr<views::InkDropMask> NewTabButton::CreateInkDropMask() const {
  return std::make_unique<views::RoundRectInkDropMask>(
      GetContentsBounds().size(), gfx::Insets(), GetCornerRadius());
}

void NewTabButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->Translate(GetContentsBounds().OffsetFromOrigin());
  PaintFill(canvas);
  PaintPlusIcon(canvas);
}

void NewTabButton::Layout() {
  ImageButton::Layout();

  // TODO(pkasting): Instead of setting this bounds rect, maybe have the
  // container match the view bounds, then undo the coordinate transforms in
  // the ink drop creation methods above.
  const gfx::Rect contents_bounds = GetContentsBounds();
  ink_drop_container_->SetBoundsRect(contents_bounds);

  focus_ring()->SetPath(
      GetBorderPath(GetContentsBounds().origin(), 1.0f, false));
}

gfx::Size NewTabButton::CalculatePreferredSize() const {
  gfx::Size size = kButtonSize;
  const auto insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void NewTabButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  const gfx::Size ink_drop_size = GetContentsBounds().size();
  GetInkDrop()->HostSizeChanged(ink_drop_size);
  UpdateInkDropMaskLayerSize(ink_drop_size);
}

bool NewTabButton::GetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);

  const float scale = GetWidget()->GetCompositor()->device_scale_factor();
  // TODO(pkasting): Fitts' Law horizontally when appropriate.
  gfx::Path border =
      GetBorderPath(GetContentsBounds().origin(), scale,
                    tab_strip_->controller()->IsFrameCondensed());
  mask->addPath(border, SkMatrix::MakeScale(1 / scale));
  return true;
}

void NewTabButton::OnWidgetDestroying(views::Widget* widget) {
#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
  feature_engagement::NewTabTrackerFactory::GetInstance()
      ->GetForProfile(tab_strip_->controller()->GetProfile())
      ->OnPromoClosed();
#endif
  new_tab_promo_observer_.Remove(widget);
  new_tab_promo_ = nullptr;
  // When the promo widget is destroyed, the NewTabButton needs to be recolored.
  SchedulePaint();
}

int NewTabButton::GetCornerRadius() const {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MAXIMUM, GetContentsBounds().size());
}

void NewTabButton::PaintFill(gfx::Canvas* canvas) const {
  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->UndoDeviceScaleFactor();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  bool has_custom_image;
  const int bg_id = tab_strip_->GetBackgroundResourceId(&has_custom_image);
  const float scale = canvas->image_scale();
  if (has_custom_image && !new_tab_promo_observer_.IsObservingSources()) {
    float x_scale = scale;
    const gfx::Rect& contents_bounds = GetContentsBounds();
    int x = GetMirroredX() + contents_bounds.x() + background_offset_;
    if (base::i18n::IsRTL()) {
      // The new tab background is mirrored in RTL mode, but the theme
      // background should never be mirrored. Mirror it here to compensate.
      x_scale = -scale;
      // Offset by |width| such that the same region is painted as if there
      // was no flip.
      x += contents_bounds.width();
    }

    canvas->InitPaintFlagsForTiling(
        *GetThemeProvider()->GetImageSkiaNamed(bg_id), x, contents_bounds.y(),
        x_scale, scale, 0, 0, SkShader::kRepeat_TileMode,
        SkShader::kRepeat_TileMode, &flags);
  } else {
    flags.setColor(GetButtonFillColor());
  }

  canvas->DrawPath(GetBorderPath(gfx::Point(), scale, false), flags);
}

void NewTabButton::PaintPlusIcon(gfx::Canvas* canvas) const {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(tab_strip_->GetTabForegroundColor(TAB_INACTIVE));
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  constexpr int kStrokeWidth = 2;
  flags.setStrokeWidth(kStrokeWidth);

  const int radius = ui::MaterialDesignController::touch_ui() ? 7 : 6;
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

SkColor NewTabButton::GetButtonFillColor() const {
  if (new_tab_promo_observer_.IsObservingSources()) {
    return GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor);
  }

  return GetThemeProvider()->GetDisplayProperty(
             ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR)
             ? tab_strip_->GetTabBackgroundColor(TAB_INACTIVE)
             : SK_ColorTRANSPARENT;
}

gfx::Path NewTabButton::GetBorderPath(const gfx::Point& origin,
                                      float scale,
                                      bool extend_to_top) const {
  gfx::PointF scaled_origin(origin);
  scaled_origin.Scale(scale);
  const float radius = GetCornerRadius() * scale;

  gfx::Path path;
  if (extend_to_top) {
    path.moveTo(scaled_origin.x(), 0);
    const float diameter = radius * 2;
    path.rLineTo(diameter, 0);
    path.rLineTo(0, scaled_origin.y() + radius);
    path.rArcTo(radius, radius, 0, SkPath::kSmall_ArcSize,
                SkPath::kCW_Direction, -diameter, 0);
    path.close();
  } else {
    path.addCircle(scaled_origin.x() + radius, scaled_origin.y() + radius,
                   radius);
  }
  return path;
}

void NewTabButton::UpdateInkDropBaseColor() {
  set_ink_drop_base_color(color_utils::BlendTowardOppositeLuma(
      GetButtonFillColor(), SK_AlphaOPAQUE));
}
