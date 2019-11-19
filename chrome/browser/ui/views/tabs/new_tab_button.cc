// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/new_tab_button.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/buildflags.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/default_theme_provider.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker_factory.h"
#endif

namespace {

// For new tab in-product help.
int GetNewTabPromoStringSpecifier() {
  static constexpr int kTextIds[] = {IDS_NEWTAB_PROMO_0, IDS_NEWTAB_PROMO_1,
                                     IDS_NEWTAB_PROMO_2};
  const std::string& str = variations::GetVariationParamValue(
      "NewTabInProductHelp", "x_promo_string");
  size_t text_specifier;
  if (!base::StringToSizeT(str, &text_specifier) ||
      text_specifier >= base::size(kTextIds)) {
    text_specifier = 0;
  }

  return kTextIds[text_specifier];
}

}  // namespace

// static
constexpr char NewTabButton::kClassName[];

// static
const gfx::Size NewTabButton::kButtonSize{28, 28};

class NewTabButton::HighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  HighlightPathGenerator() = default;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return static_cast<const NewTabButton*>(view)->GetBorderPath(
        view->GetContentsBounds().origin(), 1.0f, false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HighlightPathGenerator);
};

NewTabButton::NewTabButton(TabStrip* tab_strip, views::ButtonListener* listener)
    : views::ImageButton(listener), tab_strip_(tab_strip) {
  set_animate_on_state_change(true);
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  set_triggerable_event_flags(triggerable_event_flags() |
                              ui::EF_MIDDLE_MOUSE_BUTTON);
#endif

  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());

  SetInkDropMode(InkDropMode::ON);
  set_ink_drop_highlight_opacity(0.16f);
  set_ink_drop_visible_opacity(0.14f);

  SetInstallFocusRingOnFocus(true);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<NewTabButton::HighlightPathGenerator>());
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
  new_tab_promo_ = FeaturePromoBubbleView::CreateOwned(
      this, views::BubbleBorder::LEFT_CENTER,
      FeaturePromoBubbleView::ActivationAction::DO_NOT_ACTIVATE,
      GetNewTabPromoStringSpecifier());
  new_tab_promo_observer_.Add(new_tab_promo_->GetWidget());
  SchedulePaint();
}

void NewTabButton::CloseBubble() {
  if (new_tab_promo_)
    new_tab_promo_->CloseBubble();
}

void NewTabButton::FrameColorsChanged() {
  UpdateInkDropBaseColor();
  SchedulePaint();
}

void NewTabButton::AnimateInkDropToStateForTesting(views::InkDropState state) {
  GetInkDrop()->AnimateToState(state);
}

const char* NewTabButton::GetClassName() const {
  return kClassName;
}

void NewTabButton::AddLayerBeneathView(ui::Layer* new_layer) {
  ink_drop_container_->AddLayerBeneathView(new_layer);
}

void NewTabButton::RemoveLayerBeneathView(ui::Layer* old_layer) {
  ink_drop_container_->RemoveLayerBeneathView(old_layer);
}

void NewTabButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ImageButton::OnBoundsChanged(previous_bounds);
  ink_drop_container_->SetBoundsRect(GetLocalBounds());
}

#if defined(OS_WIN)
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
  bool destroyed = false;
  destroyed_ = &destroyed;
  views::ShowSystemMenuAtScreenPixelLocation(views::HWNDForView(this), point);
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

void NewTabButton::NotifyClick(const ui::Event& event) {
  ImageButton::NotifyClick(event);
  GetInkDrop()->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
}

void NewTabButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->Translate(GetContentsBounds().OffsetFromOrigin());
  PaintFill(canvas);
  PaintPlusIcon(canvas);
}

gfx::Size NewTabButton::CalculatePreferredSize() const {
  gfx::Size size = kButtonSize;
  const auto insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

bool NewTabButton::GetHitTestMask(SkPath* mask) const {
  DCHECK(mask);

  const float scale = GetWidget()->GetCompositor()->device_scale_factor();
  // TODO(pkasting): Fitts' Law horizontally when appropriate.
  SkPath border = GetBorderPath(GetContentsBounds().origin(), scale,
                                tab_strip_->controller()->IsFrameCondensed());
  mask->addPath(border, SkMatrix::MakeScale(1 / scale));
  return true;
}

void NewTabButton::OnWidgetDestroying(views::Widget* widget) {
#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
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

  const float scale = canvas->image_scale();
  const base::Optional<int> bg_id =
      tab_strip_->GetCustomBackgroundId(BrowserFrameActiveState::kUseCurrent);
  if (bg_id.has_value() && !new_tab_promo_observer_.IsObservingSources()) {
    float x_scale = scale;
    const gfx::Rect& contents_bounds = GetContentsBounds();
    int x = GetMirroredX() + contents_bounds.x() +
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
    flags.setColor(GetButtonFillColor());
  }

  canvas->DrawPath(GetBorderPath(gfx::Point(), scale, false), flags);
}

void NewTabButton::PaintPlusIcon(gfx::Canvas* canvas) const {
  const SkColor background_color = tab_strip_->GetTabBackgroundColor(
      TabActive::kInactive, BrowserFrameActiveState::kUseCurrent);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(tab_strip_->GetTabForegroundColor(TabActive::kInactive,
                                                   background_color));
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
             ? tab_strip_->GetTabBackgroundColor(
                   TabActive::kInactive, BrowserFrameActiveState::kUseCurrent)
             : SK_ColorTRANSPARENT;
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
  set_ink_drop_base_color(
      color_utils::GetColorWithMaxContrast(GetButtonFillColor()));
}
