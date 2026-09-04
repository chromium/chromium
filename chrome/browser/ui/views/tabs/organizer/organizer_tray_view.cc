// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/custom_corners.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/frame/shadow_frame_view.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_controls_view.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/event_monitor.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

// Shadow is used in expand-on-hover mode. Shadow radius and opacity are dynamic
// and set by the layout.
constexpr int kPanelShadowElevation = 4;
constexpr ShadowFrameView::ShadowAlpha kPanelShadowAlpha({.light_key = 0.3,
                                                          .light_ambient = 0.0,
                                                          .dark_key = 0.6,
                                                          .dark_ambient = 0.0});
}  // namespace

// ------------------------------------------------------------------
// OrganizerTrayView::Animator

// TODO(dfried): Remove in favor of BrowserAnimationController.
class OrganizerTrayView::Animator : public gfx::AnimationDelegate {
 public:
  explicit Animator(OrganizerTrayView& tray) : tray_(tray), animation_(this) {
    animation_.SetTweenType(gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED);
  }

  double GetAnimationValue() const { return animation_.GetCurrentValue(); }
  void SetAnimationValue(double value) {
    animation_.Reset(value);
    tray_->InvalidateLayout();
  }

  void Show() {
    tray_->SetVisible(true);
    animation_.SetSlideDuration(kPanelShowAnimationDuration);
    animation_.Show();
  }

  void Hide() {
    animation_.SetSlideDuration(kPanelHideAnimationDuration);
    animation_.Hide();
  }

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    tray_->InvalidateLayout();
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    if (animation->GetCurrentValue() == 0.0) {
      views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
          kCloseAnimationComplete, &*tray_);
      tray_->SetVisible(false);
    } else {
      views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
          kOpenAnimationComplete, &*tray_);
    }
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

 private:
  // Animation when opening and closing the panel.
  const raw_ref<OrganizerTrayView> tray_;
  gfx::SlideAnimation animation_;
};

// ------------------------------------------------------------------
// OrganizerTrayView::EventObserver

// Detects if mouse presses occur outside of the panel, or if the panel loses
// focus in some other way.
class OrganizerTrayView::EventObserver : public ui::EventObserver,
                                         public views::FocusChangeListener {
 public:
  explicit EventObserver(OrganizerTrayView& tray) : tray_(tray) {
    tray_->GetFocusManager()->AddFocusChangeListener(this);
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, tray.GetWidget()->GetNativeWindow(),
        {ui::EventType::kMousePressed, ui::EventType::kGestureTapDown});
  }

  EventObserver(const EventObserver&) = delete;
  EventObserver& operator=(const EventObserver&) = delete;

  ~EventObserver() override {
    tray_->GetFocusManager()->RemoveFocusChangeListener(this);
  }

  void OnEvent(const ui::Event& event) override {
    // Ignore mouse events when the panel is closed.
    if (!tray_->GetVisible()) {
      return;
    }

    if (event.type() == ui::EventType::kMousePressed ||
        event.type() == ui::EventType::kGestureTapDown) {
      if (!tray_->GetWidget()) {
        return;
      }

      auto point_in_view = event.AsLocatedEvent()->location();

      // Convert the point from the event's target to the panel's coordinates.
      views::View::ConvertPointFromWidget(&*tray_, &point_in_view);

      if (!tray_->GetLocalBounds().Contains(point_in_view)) {
        tray_->ClosePanel();
      }
    }
  }

  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    if (!tray_->GetVisible() || tray_->Contains(focused_now)) {
      return;
    }

    // If the panel is closing due to focus being lost (e.g., a tab group was
    // focused or a tab was activated), the last focused view before the panel
    // was opened should not be refocused.
    tray_->last_focused_view_before_opening_.SetView(nullptr);
    tray_->ClosePanel();
  }

 private:
  raw_ref<OrganizerTrayView> tray_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

// ------------------------------------------------------------------
// OrganizerTrayView

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(OrganizerTrayView, kTrayElementId);

OrganizerTrayView::OrganizerTrayView(BrowserWindowInterface& browser)
    : browser_(browser),
      controller_state_subscription_(
          OrganizerPanelStateController::From(&*browser_)
              ->RegisterOnStateChanged(base::BindRepeating(
                  &OrganizerTrayView::OnOrganizerPanelStateChanged,
                  base::Unretained(this)))),
      focus_search_(this, /*cycle=*/true, /*accessibility_mode=*/true),
      animator_(std::make_unique<Animator>(*this)) {
  // TODO(dfried): Remove once we actually set this value.
#if BUILDFLAG(IS_MAC)
  top_leading_exclusion_ = gfx::Size(target_width_ / 2, 0);
#endif

  SetProperty(views::kElementIdentifierKey, kTrayElementId);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetOrientation(views::LayoutOrientation::kVertical);
  SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  SetMainAxisAlignment(views::LayoutAlignment::kStart);

  // Set up the default background.
  if (auto* const browser_view =
          BrowserView::GetBrowserViewForBrowser(&browser)) {
    auto background = std::make_unique<CustomCornersBackground>(
        *this, *browser_view, organizer_panel::kOrganizerPanelBackgroundColor,
        organizer_panel::kOrganizerPanelBackgroundColor);
    CustomCornersBackground::Corners corners;
    corners[CornerOrientation::kTopLeading] = background->GetWindowCorner(true);
    corners[CornerOrientation::kBottomLeading] =
        background->GetWindowCorner(false);
    corners[CornerOrientation::kTopTrailing].type =
        CustomCornersBackground::CornerType::kRounded;
    corners[CornerOrientation::kBottomTrailing].type =
        CustomCornersBackground::CornerType::kRounded;
    background->SetCorners(corners);
    SetBackground(std::move(background));
  } else {
    CHECK_IS_TEST() << "Should only happen in unit tests.";
  }

  shadow_frame_ = AddChildView(std::make_unique<ShadowFrameView>(
      kPanelShadowElevation, kPanelShadowAlpha));
  shadow_frame_->SetProperty(views::kViewIgnoredByLayoutKey, true);

  controls_view_ = AddChildView(std::make_unique<OrganizerPanelControlsView>(
      BrowserActions::From(&*browser_)->root_action_item()));
  controls_view_->SetProperty(views::kMarginsKey,
                              organizer_panel::kOrganizerPanelControlsMargins);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  SetVisible(false);
}

OrganizerTrayView::~OrganizerTrayView() = default;

bool OrganizerTrayView::IsPositionInWindowCaption(const gfx::Point& point) {
  const auto in_controls =
      views::View::ConvertPointToTarget(this, controls_view_, point);
  return controls_view_->HitTestPoint(in_controls) &&
         controls_view_->IsPositionInWindowCaption(in_controls);
}

void OrganizerTrayView::SetTopLeadingExclusion(
    const gfx::Size& top_leading_exclusion) {
  if (top_leading_exclusion == top_leading_exclusion_) {
    return;
  }
  top_leading_exclusion_ = top_leading_exclusion;
  controls_view_->SetMinimumCrossAxisSize(std::max(
      0, top_leading_exclusion_.height() -
             controls_view_->GetProperty(views::kMarginsKey)->height()));
}

void OrganizerTrayView::SetTargetWidth(int target_width) {
  if (target_width_ == target_width) {
    return;
  }
  target_width_ = target_width;
  InvalidateLayout(/*avoid_propagate_during_layout=*/true);
}

void OrganizerTrayView::SetPanelView(std::unique_ptr<views::View> panel_view) {
  CHECK(!panel_view_);
  panel_view_ = AddChildView(std::move(panel_view));
  panel_view_->SetProperty(views::kViewIgnoredByLayoutKey, true);
}

std::unique_ptr<views::View> OrganizerTrayView::TakePanelView() {
  CHECK(panel_view_);
  panel_view_->SetProperty(views::kViewIgnoredByLayoutKey, false);
  auto result = RemoveChildViewT(panel_view_);
  panel_view_ = nullptr;
  return result;
}

// ----------------
// To be removed.

DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(OrganizerTrayView,
                                       kOpenAnimationComplete);
DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(OrganizerTrayView,
                                       kCloseAnimationComplete);

double OrganizerTrayView::GetAnimationValue() const {
  return animator_->GetAnimationValue();
}

void OrganizerTrayView::SetAnimationValueForTesting(double value) {
  animator_->SetAnimationValue(value);
}

// Set whether the panel should appear elevated with rounded borders.
void OrganizerTrayView::SetIsElevated(bool elevated) {
  if (elevated == elevated_) {
    return;
  }
  if (auto* const bg = background()->AsA<CustomCornersBackground>()) {
    bg->SetVisible(elevated);
  }
  shadow_frame_->SetVisible(elevated);
}

// ----------------

bool OrganizerTrayView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    ClosePanel();
    return true;
  }
  return false;
}

void OrganizerTrayView::AddedToWidget() {
  // This has to be done after there is a color provider, which happens after
  // attaching to a widget.
  int radius = 8;
  if (auto* const bg = background()) {
    radius = bg->AsA<CustomCornersBackground>()->default_radius();
  }
  shadow_frame_->SetShadowCornerRadius(radius);
  shadow_frame_->SetShadowVisible(true);
}

void OrganizerTrayView::VisibilityChanged(views::View* from, bool visible) {
  if (visible) {
    event_observer_ = std::make_unique<EventObserver>(*this);
    last_focused_view_before_opening_.SetView(
        GetFocusManager()->GetFocusedView());
    GetFocusManager()->SetFocusedView(this);
  } else {
    event_observer_.reset();
    if (last_focused_view_before_opening_) {
      GetFocusManager()->SetFocusedView(
          last_focused_view_before_opening_.view());
      last_focused_view_before_opening_.SetView(nullptr);
    }
  }
}

views::FocusTraversable* OrganizerTrayView::GetPaneFocusTraversable() {
  return this;
}

views::FocusSearch* OrganizerTrayView::GetFocusSearch() {
  return &focus_search_;
}

views::FocusTraversable* OrganizerTrayView::GetFocusTraversableParent() {
  return parent() ? parent()->GetFocusTraversable() : nullptr;
}

views::View* OrganizerTrayView::GetFocusTraversableParentView() {
  return this;
}

void OrganizerTrayView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  const gfx::Insets controls_margins =
      *controls_view_->GetProperty(views::kMarginsKey);

  // Shadow frame does not participate in normal layout.
  shadow_frame_->SetBoundsRect(GetLocalBounds());

  // Panel view (if present) is laid out below controls, filling remaining
  // space.
  if (panel_view_) {
    const int panel_top =
        controls_view_->bounds().bottom() + controls_margins.bottom();
    panel_view_->SetBounds(std::min(0, width() - target_width_), panel_top,
                           target_width_, height() - panel_top);
  }

  // If there's an exclusion for caption buttons and the panel is not at its
  // target open width, may need to fade out the controls to avoid overlapping
  // the caption buttons.
  double opacity = 1.0;
  const int exclusion_width = top_leading_exclusion_.width();
  const int actual_width = width();
  if (exclusion_width > 0 && actual_width < target_width_) {
    const int controls_preferred_width =
        controls_view_->GetPreferredSize().width();
    const int required_width =
        exclusion_width + controls_preferred_width + controls_margins.right();
    if (actual_width <= required_width) {
      // Not enough space to show the controls without overlapping buttons.
      opacity = 0.0;
    } else {
      // As the width goes from the required width to full width, scale opacity
      // from 0 to 1.
      opacity = (actual_width - required_width) /
                static_cast<double>(target_width_ - required_width);
    }
  }
  controls_view_->SetButtonOpacity(opacity);
}

void OrganizerTrayView::ClosePanel() {
  // Ignore if the panel is already animating closed.
  if (!GetVisible() || !OrganizerPanelStateController::From(&*browser_)
                            ->IsOrganizerPanelVisible()) {
    return;
  }

  if (auto* const action = actions::ActionManager::Get().FindAction(
          kActionToggleOrganizerPanel,
          BrowserActions::From(&*browser_)->root_action_item())) {
    action->InvokeAction();
  }
}

void OrganizerTrayView::OnOrganizerPanelStateChanged(
    OrganizerPanelStateController* state_controller) {
  controls_view_->UpdateTooltipText();
  TooltipTextChanged();

  if (state_controller->IsOrganizerPanelVisible()) {
    animator_->Show();
  } else {
    animator_->Hide();
  }
}

BEGIN_METADATA(OrganizerTrayView)
END_METADATA
