// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_controls_view.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view_layout.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/public/features.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/event_monitor.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_shadow.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

constexpr int kClipRectMarginForShadow = 32;
constexpr int kProjectPanelRightCornerRadius = 16;
constexpr int kShadowElevation = 2;

constexpr base::TimeDelta kPanelShowAnimationDuration = base::Milliseconds(250);
constexpr base::TimeDelta kPanelHideAnimationDuration = base::Milliseconds(200);

static bool disable_animations_for_testing_ = false;

}  // namespace

OrganizerPanelView::OrganizerPanelView(
    BrowserWindowInterface* browser,
    actions::ActionItem* root_action_item,
    OrganizerPanelStateController* state_controller)
    : browser_(browser),
      root_action_item_(root_action_item),
      action_view_controller_(std::make_unique<views::ActionViewController>()),
      state_controller_(state_controller),
      resize_animation_(this),
      focus_search_(std::make_unique<views::FocusSearch>(this,
                                                         /*cycle=*/true,
                                                         /*accessibility_mode=*/
                                                         true)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  content_container_ = AddChildView(std::make_unique<views::View>());
  content_container_->SetPaintToLayer();
  content_container_->layer()->SetFillsBoundsOpaquely(false);

  content_shadow_ =
      std::make_unique<views::ViewShadow>(content_container_, kShadowElevation);
  content_shadow_->SetRoundedCornerRadius(kProjectPanelRightCornerRadius);

  SetIsElevated(true);

  controls_view_ = content_container_->AddChildView(
      std::make_unique<OrganizerPanelControlsView>(root_action_item_.get()));

  if (browser_ && browser_->GetProfile()) {
    auto web_view = std::make_unique<views::WebView>(browser_->GetProfile());
    web_view->LoadInitialURL(GURL(chrome::kChromeUIOrganizerPanelURL));
    web_view_ = content_container_->AddChildView(std::move(web_view));
  }

  content_container_->SetLayoutManager(
      std::make_unique<OrganizerPanelViewLayout>(controls_view_));

  resize_animation_.SetTweenType(gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  auto& accessibility = GetViewAccessibility();
  accessibility.SetRole(ax::mojom::Role::kPane);
  accessibility.SetName(l10n_util::GetStringUTF16(IDS_ORGANIZER_PANEL));
  SetFocusBehavior(FocusBehavior::NEVER);

  SetVisible(false);
  SetPreferredSize(gfx::Size(organizer_panel::kOrganizerPanelMinWidth, 0));
  SetProperty(views::kElementIdentifierKey, kOrganizerPanelViewElementId);
}

OrganizerPanelView::~OrganizerPanelView() = default;

bool OrganizerPanelView::IsPositionInWindowCaption(const gfx::Point& point) {
  gfx::Point point_in_target = point;
  views::View::ConvertPointToTarget(this, controls_view_, &point_in_target);
  if (controls_view_->HitTestPoint(point_in_target)) {
    return controls_view_->IsPositionInWindowCaption(point_in_target);
  }

  return false;
}

void OrganizerPanelView::OnOrganizerPanelStateChanged(
    OrganizerPanelStateController* state_controller) {
  TooltipTextChanged();
  controls_view_->UpdateTooltipText();

  const bool visible = state_controller->IsOrganizerPanelVisible();

  if (visible) {
    views::Widget* widget = GetWidget();
    if (widget && widget->GetNativeWindow()) {
      event_monitor_ = views::EventMonitor::CreateWindowMonitor(
          &mouse_event_handler_, widget->GetNativeWindow(),
          {ui::EventType::kMousePressed, ui::EventType::kGestureTapDown});
    }

    if (!observing_focus_manager_ && GetFocusManager()) {
      GetFocusManager()->AddFocusChangeListener(this);
      last_focused_view_before_opening_.SetView(
          GetFocusManager()->GetFocusedView());
      observing_focus_manager_ = true;
    }

    last_opened_time_ = base::TimeTicks::Now();
  } else {
    if (observing_focus_manager_ && GetFocusManager()) {
      GetFocusManager()->RemoveFocusChangeListener(this);
      if (last_focused_view_before_opening_) {
        GetFocusManager()->SetFocusedView(
            last_focused_view_before_opening_.view());
        last_focused_view_before_opening_.SetView(nullptr);
      }
      observing_focus_manager_ = false;
    }
    event_monitor_.reset();

    base::TimeDelta open_duration = base::TimeTicks::Now() - last_opened_time_;
    base::UmaHistogramCustomCounts("Projects.ProjectsPanel.TimeOpen",
                                   open_duration.InSeconds(), 1,
                                   base::Minutes(5).InSeconds(), 50);
  }

  if (disable_animations_for_testing_) {
    resize_animation_.SetSlideDuration(base::TimeDelta());
    resize_animation_.Reset(/*value=*/visible ? 1.0 : 0.0);
    SetVisible(visible);
    if (!visible) {
      AnimationEnded(&resize_animation_);
    }
  } else {
    if (visible) {
      SetVisible(true);
      resize_animation_.SetSlideDuration(kPanelShowAnimationDuration);
      resize_animation_.Show();
    } else {
      resize_animation_.SetSlideDuration(kPanelHideAnimationDuration);
      resize_animation_.Hide();
    }
  }

  if (visible && GetFocusManager()) {
    GetFocusManager()->SetFocusedView(this);
  }
}

double OrganizerPanelView::GetResizeAnimationValue() const {
  return resize_animation_.GetCurrentValue();
}

void OrganizerPanelView::SetTargetWidth(int target_width) {
  if (target_width_ == target_width) {
    return;
  }
  target_width_ = target_width;

  InvalidateLayout();
}

void OrganizerPanelView::SetIsElevated(bool elevated) {
  if (elevated_ == elevated) {
    return;
  }
  elevated_ = elevated;

  const int elevation = elevated_ ? kShadowElevation : 0;
  content_shadow_->shadow()->SetElevation(elevation);

  const int corner_radius = elevated_ ? kProjectPanelRightCornerRadius : 0;
  gfx::RoundedCornersF radii;
  if (base::i18n::IsRTL()) {
    radii = gfx::RoundedCornersF(corner_radius, 0, 0, corner_radius);
  } else {
    radii = gfx::RoundedCornersF(0, corner_radius, corner_radius, 0);
  }

  content_container_->layer()->SetRoundedCornerRadius(radii);
  content_container_->SetBackground(views::CreateRoundedRectBackground(
      organizer_panel::kOrganizerPanelBackgroundColor, radii));

  InvalidateLayout();
}

void OrganizerPanelView::Layout(PassKey) {
  const int visible_width = width();
  content_container_->SetBounds(-(target_width_ - visible_width), 0,
                                target_width_, height());

  // The content_container_ slides in from the edge of the window. In LTR this
  // is the left edge, and it should be clipped to that edge. However, we still
  // want the shadow to be visible on the opposite side, so we set a clip rect
  // that extends slightly beyond that edge.
  gfx::Rect clip_rect(0, 0, target_width_, height());
  if (base::i18n::IsRTL()) {
    clip_rect.Inset(gfx::Insets::TLBR(0, -kClipRectMarginForShadow, 0, 0));
  } else {
    clip_rect.Inset(gfx::Insets::TLBR(0, 0, 0, -kClipRectMarginForShadow));
  }
  layer()->SetClipRect(clip_rect);
}

void OrganizerPanelView::RemovedFromWidget() {
  if (observing_focus_manager_ && GetFocusManager()) {
    GetFocusManager()->RemoveFocusChangeListener(this);
    observing_focus_manager_ = false;
  }
}

bool OrganizerPanelView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    ClosePanel();
    return true;
  }
  return false;
}

views::FocusTraversable* OrganizerPanelView::GetPaneFocusTraversable() {
  return this;
}

views::FocusSearch* OrganizerPanelView::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* OrganizerPanelView::GetFocusTraversableParent() {
  return parent() ? parent()->GetFocusTraversable() : nullptr;
}

views::View* OrganizerPanelView::GetFocusTraversableParentView() {
  return this;
}

void OrganizerPanelView::AnimationProgressed(const gfx::Animation* animation) {
#if BUILDFLAG(IS_MAC)
  // On Mac, start fading in the close button when the panel has completed half
  // of its opening animation. Similarly when closing, fade out the button until
  // the panel has completed half of its closing animation.
  if (controls_view_) {
    const double value = animation->GetCurrentValue();
    controls_view_->SetButtonOpacity(std::max(0.0, (value - 0.5) * 2.0));
  }
#endif
  InvalidateLayout();
}

void OrganizerPanelView::AnimationEnded(const gfx::Animation* animation) {
  if (animation->GetCurrentValue() == 0.0) {
    SetVisible(false);
    if (on_close_animation_ended_callback_) {
      std::move(on_close_animation_ended_callback_).Run();
    }
  }
}

// We must also call AnimationEnded when an animation is canceled (which happens
// when the view is destroyed or a new animation is started mid-flight) to
// guarantee that the state is properly set to hidden.
void OrganizerPanelView::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

// static
views::View* OrganizerPanelView::web_view_for_testing() {
  return web_view_;
}

void OrganizerPanelView::disable_animations_for_testing() {
  disable_animations_for_testing_ = true;
}

void OrganizerPanelView::ClosePanel(bool caused_by_focus_lost) {
  // If the panel is closing due to focus being lost (e.g., a tab group was
  // focused or a tab was activated), the last focused view before the panel was
  // opened should not be refocused.
  if (caused_by_focus_lost) {
    last_focused_view_before_opening_.SetView(nullptr);
  }

  // Ignore if the panel is already animating closed.
  if (!GetVisible() || resize_animation_.IsClosing()) {
    return;
  }

  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionToggleOrganizerPanel, root_action_item_);
  if (action_item) {
    action_item->InvokeAction();
  }
}

OrganizerPanelView::MouseEventHandler::MouseEventHandler(
    OrganizerPanelView* owning_view)
    : owning_view_(owning_view) {}

OrganizerPanelView::MouseEventHandler::~MouseEventHandler() = default;

void OrganizerPanelView::MouseEventHandler::OnEvent(const ui::Event& event) {
  // Ignore mouse events when the panel is closed.
  if (!owning_view_->GetVisible()) {
    return;
  }

  if (event.type() == ui::EventType::kMousePressed ||
      event.type() == ui::EventType::kGestureTapDown) {
    if (!owning_view_->GetWidget()) {
      return;
    }

    auto point_in_view = event.AsLocatedEvent()->location();

    // Convert the point from the event's target to the panel's coordinates.
    views::View::ConvertPointFromWidget(owning_view_, &point_in_view);

    if (!owning_view_->GetLocalBounds().Contains(point_in_view)) {
      owning_view_->ClosePanel();
    }
  }
}

void OrganizerPanelView::OnWillChangeFocus(views::View* focused_before,
                                           views::View* focused_now) {}

void OrganizerPanelView::OnDidChangeFocus(views::View* focused_before,
                                          views::View* focused_now) {
  if (!GetVisible() || Contains(focused_now)) {
    return;
  }

  ClosePanel();
}

BEGIN_METADATA(OrganizerPanelView)
END_METADATA
