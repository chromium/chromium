// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/debug/alias.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/alert_indicator.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_style.h"
#include "chrome/browser/ui/views/touch_uma/touch_uma.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

using base::UserMetricsAction;
using MD = ui::MaterialDesignController;

namespace {

// When a non-pinned tab becomes a pinned tab the width of the tab animates. If
// the width of a pinned tab is at least kPinnedTabExtraWidthToRenderAsNormal
// larger than the desired pinned tab width then the tab is rendered as a normal
// tab. This is done to avoid having the title immediately disappear when
// transitioning a tab from normal to pinned tab.
constexpr int kPinnedTabExtraWidthToRenderAsNormal = 30;

// Opacity of the active tab background painted over inactive selected tabs.
constexpr float kSelectedTabOpacity = 0.75f;

// Helper functions ------------------------------------------------------------

// Returns the coordinate for an object of size |item_size| centered in a region
// of size |size|, biasing towards placing any extra space ahead of the object.
int Center(int size, int item_size) {
  int extra_space = size - item_size;
  // Integer division below truncates, thus effectively "rounding toward zero";
  // to always place extra space ahead of the object, we want to round towards
  // positive infinity, which means we need to bias the division only when the
  // size difference is positive.  (Adding one unconditionally will stack with
  // the truncation if |extra_space| is negative, resulting in off-by-one
  // errors.)
  if (extra_space > 0)
    ++extra_space;
  return extra_space / 2;
}

}  // namespace

// Tab -------------------------------------------------------------------------

// static
const char Tab::kViewClassName[] = "Tab";

Tab::Tab(TabController* controller, gfx::AnimationContainer* container)
    : controller_(controller),
      pulse_animation_(this),
      animation_container_(container),
      title_(new views::Label()),
      title_animation_(this),
      hover_controller_(this) {
  DCHECK(controller);

  tab_style_.reset(TabStyle::CreateForTab(this));

  // So we get don't get enter/exit on children and don't prematurely stop the
  // hover.
  set_notify_enter_exit_on_child(true);

  set_id(VIEW_ID_TAB);

  // This will cause calls to GetContentsBounds to return only the rectangle
  // inside the tab shape, rather than to its extents.
  SetBorder(views::CreateEmptyBorder(tab_style()->GetContentsInsets()));

  title_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_->SetElideBehavior(gfx::FADE_TAIL);
  title_->SetHandlesTooltips(false);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetText(CoreTabHelper::GetDefaultTitle());
  AddChildView(title_);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  icon_ = new TabIcon;
  AddChildView(icon_);

  alert_indicator_ = new AlertIndicator(this);
  AddChildView(alert_indicator_);

  // Unretained is safe here because this class outlives its close button, and
  // the controller outlives this Tab.
  close_button_ = new TabCloseButton(
      this, base::BindRepeating(&TabController::OnMouseEventInTab,
                                base::Unretained(controller_)));
  AddChildView(close_button_);

  set_context_menu_controller(this);

  constexpr int kPulseDurationMs = 200;
  pulse_animation_.SetSlideDuration(kPulseDurationMs);
  pulse_animation_.SetContainer(animation_container_.get());

  title_animation_.SetDuration(base::TimeDelta::FromMilliseconds(100));
  title_animation_.SetContainer(animation_container_.get());

  hover_controller_.SetAnimationContainer(animation_container_.get());

  // Enable keyboard focus.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  focus_ring_ = views::FocusRing::Install(this);
}

Tab::~Tab() {
}

void Tab::AnimationEnded(const gfx::Animation* animation) {
  if (animation == &title_animation_)
    title_->SetBoundsRect(target_title_bounds_);
  else
    SchedulePaint();
}

void Tab::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &title_animation_) {
    title_->SetBoundsRect(gfx::Tween::RectValueBetween(
        gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                   animation->GetCurrentValue()),
        start_title_bounds_, target_title_bounds_));
    return;
  }

  // Ignore if the pulse animation is being performed on active tab because
  // it repaints the same image. See PaintTab().
  if (animation == &pulse_animation_ && IsActive())
    return;

  SchedulePaint();
}

void Tab::AnimationCanceled(const gfx::Animation* animation) {
  SchedulePaint();
}

void Tab::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (!alert_indicator_ || !alert_indicator_->visible())
    base::RecordAction(UserMetricsAction("CloseTab_NoAlertIndicator"));
  else if (data_.alert_state == TabAlertState::AUDIO_PLAYING)
    base::RecordAction(UserMetricsAction("CloseTab_AudioIndicator"));
  else
    base::RecordAction(UserMetricsAction("CloseTab_RecordingIndicator"));

  const CloseTabSource source =
      (event.type() == ui::ET_MOUSE_RELEASED &&
       !(event.flags() & ui::EF_FROM_TOUCH)) ? CLOSE_TAB_FROM_MOUSE
                                             : CLOSE_TAB_FROM_TOUCH;
  DCHECK_EQ(close_button_, sender);
  controller_->CloseTab(this, source);
  if (event.type() == ui::ET_GESTURE_TAP)
    TouchUMA::RecordGestureAction(TouchUMA::kGestureTabCloseTap);
}

void Tab::ShowContextMenuForView(views::View* source,
                                 const gfx::Point& point,
                                 ui::MenuSourceType source_type) {
  if (!closing_)
    controller_->ShowContextMenuForTab(this, point, source_type);
}

bool Tab::GetHitTestMask(gfx::Path* mask) const {
  // When the window is maximized we don't want to shave off the edges or top
  // shadow of the tab, such that the user can click anywhere along the top
  // edge of the screen to select a tab. Ditto for immersive fullscreen.
  *mask = tab_style()->GetPath(
      TabStyle::PathType::kHitTest,
      GetWidget()->GetCompositor()->device_scale_factor(),
      /* force_active */ false, TabStyle::RenderUnits::kDips);
  return true;
}

void Tab::Layout() {
  const gfx::Rect contents_rect = GetContentsBounds();

  const bool was_showing_icon = showing_icon_;
  UpdateIconVisibility();

  int start = contents_rect.x();
  if (extra_padding_before_content_) {
    constexpr int kExtraLeftPaddingToBalanceCloseButtonPadding = 4;
    start += kExtraLeftPaddingToBalanceCloseButtonPadding;
  }

  // The bounds for the favicon will include extra width for the attention
  // indicator, but visually it will be smaller at kFaviconSize wide.
  gfx::Rect favicon_bounds(start, contents_rect.y(), 0, 0);
  if (showing_icon_) {
    // Height should go to the bottom of the tab for the crashed tab animation
    // to pop out of the bottom.
    favicon_bounds.set_y(contents_rect.y() +
                         Center(contents_rect.height(), gfx::kFaviconSize));
    favicon_bounds.set_size(
        gfx::Size(icon_->GetPreferredSize().width(),
                  contents_rect.height() - favicon_bounds.y()));
    if (center_icon_) {
      // When centering the favicon, the favicon is allowed to escape the normal
      // contents rect.
      favicon_bounds.set_x(Center(width(), gfx::kFaviconSize));
    } else {
      MaybeAdjustLeftForPinnedTab(&favicon_bounds, gfx::kFaviconSize);
    }
  }
  icon_->SetBoundsRect(favicon_bounds);
  icon_->SetVisible(showing_icon_);

  const int after_title_padding = GetLayoutConstant(TAB_AFTER_TITLE_PADDING);

  int close_x = contents_rect.right();
  if (showing_close_button_) {
    // If the ratio of the close button size to tab width exceeds the maximum.
    // The close button should be as large as possible so that there is a larger
    // hit-target for touch events. So the close button bounds extends to the
    // edges of the tab. However, the larger hit-target should be active only
    // for touch events, and the close-image should show up in the right place.
    // So a border is added to the button with necessary padding. The close
    // button (Tab::TabCloseButton) makes sure the padding is a hit-target only
    // for touch events.
    // TODO(pkasting): The padding should maybe be removed, see comments in
    // TabCloseButton::TargetForRect().
    close_button_->SetBorder(views::NullBorder());
    const gfx::Size close_button_size(close_button_->GetPreferredSize());
    const int top = contents_rect.y() +
                    Center(contents_rect.height(), close_button_size.height());
    // Clamp the close button position to "centered within the tab"; this should
    // only have an effect when animating in a new active tab, which might start
    // out narrower than the minimum active tab width.
    close_x = std::max(contents_rect.right() - close_button_size.width(),
                       Center(width(), close_button_size.width()));
    const int left = std::min(after_title_padding, close_x);
    close_button_->SetPosition(gfx::Point(close_x - left, 0));
    const int bottom = height() - close_button_size.height() - top;
    const int right =
        std::max(0, width() - (close_x + close_button_size.width()));
    close_button_->SetBorder(
        views::CreateEmptyBorder(top, left, bottom, right));
    close_button_->SizeToPreferredSize();
    // Re-layout the close button so it can recompute its focus ring if needed:
    // SizeToPreferredSize() will not necessarily re-Layout the View if only its
    // interior margins have changed (which this logic does), but the focus ring
    // still needs to be updated because it doesn't want to encompass the
    // interior margins.
    close_button_->Layout();
  }
  close_button_->SetVisible(showing_close_button_);

  if (showing_alert_indicator_) {
    int right = contents_rect.right();
    if (showing_close_button_) {
      right = close_x;
      if (extra_alert_indicator_padding_)
        right -= MD::touch_ui() ? 8 : 6;
    }
    const gfx::Size image_size = alert_indicator_->GetPreferredSize();
    gfx::Rect bounds(
        std::max(contents_rect.x(), right - image_size.width()),
        contents_rect.y() + Center(contents_rect.height(), image_size.height()),
        image_size.width(), image_size.height());
    if (center_icon_) {
      // When centering the alert icon, it is allowed to escape the normal
      // contents rect.
      bounds.set_x(Center(width(), bounds.width()));
    } else {
      MaybeAdjustLeftForPinnedTab(&bounds, bounds.width());
    }
    alert_indicator_->SetBoundsRect(bounds);
  }
  alert_indicator_->SetVisible(showing_alert_indicator_);

  // Size the title to fill the remaining width and use all available height.
  bool show_title = ShouldRenderAsNormalTab();
  if (show_title) {
    int title_left = start;
    if (showing_icon_) {
      // When computing the spacing from the favicon, don't count the actual
      // icon view width (which will include extra room for the alert
      // indicator), but rather the normal favicon width which is what it will
      // look like.
      const int after_favicon = favicon_bounds.x() + gfx::kFaviconSize +
                                GetLayoutConstant(TAB_PRE_TITLE_PADDING);
      title_left = std::max(title_left, after_favicon);
    }
    int title_right = contents_rect.right();
    if (showing_alert_indicator_) {
      title_right = alert_indicator_->x() - after_title_padding;
    } else if (showing_close_button_) {
      // Allow the title to overlay the close button's empty border padding.
      title_right = close_x - after_title_padding;
    }
    const int title_width = std::max(title_right - title_left, 0);
    // The Label will automatically center the font's cap height within the
    // provided vertical space.
    const gfx::Rect title_bounds(title_left, contents_rect.y(), title_width,
                                 contents_rect.height());
    show_title = title_width > 0;

    if (title_bounds != target_title_bounds_) {
      target_title_bounds_ = title_bounds;
      if (was_showing_icon == showing_icon_ || title_->bounds().IsEmpty() ||
          title_bounds.IsEmpty()) {
        title_animation_.Stop();
        title_->SetBoundsRect(title_bounds);
      } else if (!title_animation_.is_animating()) {
        start_title_bounds_ = title_->bounds();
        title_animation_.Start();
      }
    }
  }
  title_->SetVisible(show_title);

  if (focus_ring_)
    focus_ring_->Layout();
}

const char* Tab::GetClassName() const {
  return kViewClassName;
}

void Tab::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Update focus ring path.
  const SkPath path = tab_style_->GetPath(TabStyle::PathType::kHighlight, 1.0);
  SetProperty(views::kHighlightPathKey, new SkPath(path));
}

bool Tab::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE && !IsSelected()) {
    controller_->SelectTab(this);
    return true;
  }

  return false;
}

namespace {
bool IsSelectionModifierDown(const ui::MouseEvent& event) {
#if defined(OS_MACOSX)
  return event.IsCommandDown();
#else
  return event.IsControlDown();
#endif
}
}  // namespace

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  controller_->OnMouseEventInTab(this, event);

  // Allow a right click from touch to drag, which corresponds to a long click.
  if (event.IsOnlyLeftMouseButton() ||
      (event.IsOnlyRightMouseButton() && event.flags() & ui::EF_FROM_TOUCH)) {
    ui::ListSelectionModel original_selection;
    original_selection = controller_->GetSelectionModel();
    // Changing the selection may cause our bounds to change. If that happens
    // the location of the event may no longer be valid. Create a copy of the
    // event in the parents coordinate, which won't change, and recreate an
    // event after changing so the coordinates are correct.
    ui::MouseEvent event_in_parent(event, static_cast<View*>(this), parent());
    if (controller_->SupportsMultipleSelection()) {
      if (event.IsShiftDown() && IsSelectionModifierDown(event)) {
        controller_->AddSelectionFromAnchorTo(this);
      } else if (event.IsShiftDown()) {
        controller_->ExtendSelectionTo(this);
      } else if (IsSelectionModifierDown(event)) {
        controller_->ToggleSelected(this);
        if (!IsSelected()) {
          // Don't allow dragging non-selected tabs.
          return false;
        }
      } else if (!IsSelected()) {
        controller_->SelectTab(this);
        base::RecordAction(UserMetricsAction("SwitchTab_Click"));
      }
    } else if (!IsSelected()) {
      controller_->SelectTab(this);
      base::RecordAction(UserMetricsAction("SwitchTab_Click"));
    }
    ui::MouseEvent cloned_event(event_in_parent, parent(),
                                static_cast<View*>(this));
    controller_->MaybeStartDrag(this, cloned_event, original_selection);
  }
  return true;
}

bool Tab::OnMouseDragged(const ui::MouseEvent& event) {
  controller_->ContinueDrag(this, event);
  return true;
}

void Tab::OnMouseReleased(const ui::MouseEvent& event) {
  controller_->OnMouseEventInTab(this, event);

  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  // In some cases, ending the drag will schedule the tab for destruction; if
  // so, bail immediately, since our members are already dead and we shouldn't
  // do anything else except drop the tab where it is.
  if (controller_->EndDrag(END_DRAG_COMPLETE))
    return;

  // Close tab on middle click, but only if the button is released over the tab
  // (normal windows behavior is to discard presses of a UI element where the
  // releases happen off the element).
  if (event.IsMiddleMouseButton()) {
    if (HitTestPoint(event.location())) {
      controller_->CloseTab(this, CLOSE_TAB_FROM_MOUSE);
    } else if (closing_) {
      // We're animating closed and a middle mouse button was pushed on us but
      // we don't contain the mouse anymore. We assume the user is clicking
      // quicker than the animation and we should close the tab that falls under
      // the mouse.
      gfx::Point location_in_parent = event.location();
      ConvertPointToTarget(this, parent(), &location_in_parent);
      Tab* closest_tab = controller_->GetTabAt(location_in_parent);
      if (closest_tab)
        controller_->CloseTab(closest_tab, CLOSE_TAB_FROM_MOUSE);
    }
  } else if (event.IsOnlyLeftMouseButton() && !event.IsShiftDown() &&
             !IsSelectionModifierDown(event)) {
    // If the tab was already selected mouse pressed doesn't change the
    // selection. Reset it now to handle the case where multiple tabs were
    // selected.
    controller_->SelectTab(this);
  }
}

void Tab::OnMouseCaptureLost() {
  controller_->EndDrag(END_DRAG_CAPTURE_LOST);
}

void Tab::OnMouseMoved(const ui::MouseEvent& event) {
  hover_controller_.SetLocation(event.location());
  controller_->OnMouseEventInTab(this, event);
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_hovered_ = true;
  hover_controller_.SetSubtleOpacityScale(
      controller_->GetHoverOpacityForRadialHighlight());
  hover_controller_.Show(GlowHoverController::SUBTLE);
  UpdateForegroundColors();
  Layout();
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  mouse_hovered_ = false;
  hover_controller_.Hide();
  UpdateForegroundColors();
  Layout();
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      // TAP_DOWN is only dispatched for the first touch point.
      DCHECK_EQ(1, event->details().touch_points());

      // See comment in OnMousePressed() as to why we copy the event.
      ui::GestureEvent event_in_parent(*event, static_cast<View*>(this),
                                       parent());
      ui::ListSelectionModel original_selection;
      original_selection = controller_->GetSelectionModel();
      tab_activated_with_last_tap_down_ = !IsActive();
      if (!IsSelected())
        controller_->SelectTab(this);
      gfx::Point loc(event->location());
      views::View::ConvertPointToScreen(this, &loc);
      ui::GestureEvent cloned_event(event_in_parent, parent(),
                                    static_cast<View*>(this));
      controller_->MaybeStartDrag(this, cloned_event, original_selection);
      break;
    }

    case ui::ET_GESTURE_END:
      controller_->EndDrag(END_DRAG_COMPLETE);
      break;

    case ui::ET_GESTURE_SCROLL_UPDATE:
      controller_->ContinueDrag(this, *event);
      break;

    default:
      break;
  }
  event->SetHandled();
}

bool Tab::GetTooltipText(const gfx::Point& p, base::string16* tooltip) const {
  // Note: Anything that affects the tooltip text should be accounted for when
  // calling TooltipTextChanged() from Tab::SetData().
  *tooltip = GetTooltipText(data_.title, data_.alert_state);
  return !tooltip->empty();
}

bool Tab::GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* origin) const {
  origin->set_x(title_->x() + 10);
  origin->set_y(-4);
  return true;
}

void Tab::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTab;
  node_data->AddState(ax::mojom::State::kMultiselectable);
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              IsSelected());

  base::string16 name = controller_->GetAccessibleTabName(this);
  if (!name.empty()) {
    node_data->SetName(name);
  } else {
    // Under some conditions, |GetAccessibleTabName| returns an empty string.
    node_data->SetNameExplicitlyEmpty();
  }
}

gfx::Size Tab::CalculatePreferredSize() const {
  return gfx::Size(TabStyle::GetStandardWidth(), GetLayoutConstant(TAB_HEIGHT));
}

void Tab::PaintChildren(const views::PaintInfo& info) {
  // Clip children based on the tab's fill path.  This has no effect except when
  // the tab is too narrow to completely show even one icon, at which point this
  // serves to clip the favicon.
  ui::ClipRecorder clip_recorder(info.context());
  // The paint recording scale for tabs is consistent along the x and y axis.
  const float paint_recording_scale = info.paint_recording_scale_x();

  const gfx::Path clip_path = tab_style()->GetPath(
      TabStyle::PathType::kInteriorClip, paint_recording_scale);

  clip_recorder.ClipPathWithAntiAliasing(clip_path);
  View::PaintChildren(info);
}

void Tab::OnPaint(gfx::Canvas* canvas) {
  gfx::Path clip;
  if (!controller_->ShouldPaintTab(this, canvas->image_scale(), &clip))
    return;

  tab_style()->PaintTab(canvas, clip);
}

void Tab::AddedToWidget() {
  UpdateForegroundColors();
}

void Tab::OnThemeChanged() {
  UpdateForegroundColors();
}

void Tab::SetClosing(bool closing) {
  closing_ = closing;
  ActiveStateChanged();

  if (closing) {
    // When closing, sometimes DCHECK fails because
    // cc::Layer::IsPropertyChangeAllowed() returns false. Deleting
    // the focus ring fixes this. TODO(collinbaker): investigate why
    // this happens.
    focus_ring_.reset();
  }
}

SkColor Tab::GetAlertIndicatorColor(TabAlertState state) const {
  const bool touch_ui = MD::touch_ui();
  // If theme provider is not yet available, return the default button
  // color.
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return button_color_;

  switch (state) {
    case TabAlertState::AUDIO_PLAYING:
    case TabAlertState::AUDIO_MUTING:
      return touch_ui ? theme_provider->GetColor(
                            ThemeProperties::COLOR_TAB_ALERT_AUDIO)
                      : button_color_;
    case TabAlertState::MEDIA_RECORDING:
    case TabAlertState::DESKTOP_CAPTURING:
      return theme_provider->GetColor(
          ThemeProperties::COLOR_TAB_ALERT_RECORDING);
    case TabAlertState::TAB_CAPTURING:
      return touch_ui ? theme_provider->GetColor(
                            ThemeProperties::COLOR_TAB_ALERT_CAPTURING)
                      : button_color_;
    case TabAlertState::PIP_PLAYING:
      return theme_provider->GetColor(ThemeProperties::COLOR_TAB_PIP_PLAYING);
    case TabAlertState::BLUETOOTH_CONNECTED:
    case TabAlertState::USB_CONNECTED:
    case TabAlertState::NONE:
      return button_color_;
    default:
      NOTREACHED();
      return button_color_;
  }
}

bool Tab::IsActive() const {
  return controller_->IsActiveTab(this);
}

void Tab::ActiveStateChanged() {
  UpdateTabIconNeedsAttentionBlocked();
  UpdateForegroundColors();
  Layout();
}

void Tab::AlertStateChanged() {
  Layout();
}

void Tab::FrameColorsChanged() {
  UpdateForegroundColors();
  SchedulePaint();
}

void Tab::SelectedStateChanged() {
  UpdateForegroundColors();
}

bool Tab::IsSelected() const {
  return controller_->IsTabSelected(this);
}

void Tab::SetData(TabRendererData data) {
  DCHECK(GetWidget());

  if (data_ == data)
    return;

  TabRendererData old(std::move(data_));
  data_ = std::move(data);

  // Icon updating must be done first because the title depends on whether the
  // loading animation is showing.
  icon_->SetIcon(data_.url, data_.favicon);
  icon_->SetNetworkState(data_.network_state, data_.should_hide_throbber);
  icon_->SetCanPaintToLayer(controller_->CanPaintThrobberToLayer());
  icon_->SetIsCrashed(data_.IsCrashed());
  UpdateTabIconNeedsAttentionBlocked();

  base::string16 title = data_.title;
  if (title.empty()) {
    title = icon_->ShowingLoadingAnimation()
                ? l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE)
                : CoreTabHelper::GetDefaultTitle();
  } else {
    Browser::FormatTitleForDisplay(&title);
  }
  title_->SetText(title);

  if (data_.alert_state != old.alert_state)
    alert_indicator_->TransitionToAlertState(data_.alert_state);
  if (old.pinned != data_.pinned)
    showing_alert_indicator_ = false;

  if (data_.alert_state != old.alert_state || data_.title != old.title)
    TooltipTextChanged();

  Layout();
  SchedulePaint();
}

void Tab::StepLoadingAnimation() {
  icon_->StepLoadingAnimation();

  // Update the layering if necessary.
  //
  // TODO(brettw) this design should be changed to be a push state when the tab
  // can't be painted to a layer, rather than continually polling the
  // controller about the state and reevaulating that state in the icon. This
  // is both overly aggressive and wasteful in the common case, and not
  // frequent enough in other cases since the state can be updated and the tab
  // painted before the animation is stepped.
  icon_->SetCanPaintToLayer(controller_->CanPaintThrobberToLayer());
}

void Tab::StartPulse() {
  pulse_animation_.StartThrobbing(std::numeric_limits<int>::max());
}

void Tab::StopPulse() {
  pulse_animation_.Stop();
}

void Tab::SetTabNeedsAttention(bool attention) {
  icon_->SetAttention(TabIcon::AttentionType::kTabWantsAttentionStatus,
                      attention);
  SchedulePaint();
}

// static
base::string16 Tab::GetTooltipText(const base::string16& title,
                                   TabAlertState alert_state) {
  if (alert_state == TabAlertState::NONE)
    return title;

  base::string16 result = title;
  if (!result.empty())
    result.append(1, '\n');
  switch (alert_state) {
    case TabAlertState::AUDIO_PLAYING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_PLAYING));
      break;
    case TabAlertState::AUDIO_MUTING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_MUTING));
      break;
    case TabAlertState::MEDIA_RECORDING:
      result.append(l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_MEDIA_RECORDING));
      break;
    case TabAlertState::TAB_CAPTURING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_TAB_CAPTURING));
      break;
    case TabAlertState::BLUETOOTH_CONNECTED:
      result.append(l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_BLUETOOTH_CONNECTED));
      break;
    case TabAlertState::USB_CONNECTED:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_USB_CONNECTED));
      break;
    case TabAlertState::PIP_PLAYING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_PIP_PLAYING));
      break;
    case TabAlertState::DESKTOP_CAPTURING:
      result.append(l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_DESKTOP_CAPTURING));
      break;
    case TabAlertState::NONE:
      NOTREACHED();
      break;
  }
  return result;
}

void Tab::MaybeAdjustLeftForPinnedTab(gfx::Rect* bounds,
                                      int visual_width) const {
  if (ShouldRenderAsNormalTab())
    return;
  const int pinned_width = TabStyle::GetPinnedWidth();
  const int ideal_delta = width() - pinned_width;
  const int ideal_x = (pinned_width - visual_width) / 2;
  // TODO(pkasting): https://crbug.com/533570  This code is broken when the
  // current width is less than the pinned width.
  bounds->set_x(
      bounds->x() +
      gfx::ToRoundedInt(
          (1 - static_cast<float>(ideal_delta) /
                   static_cast<float>(kPinnedTabExtraWidthToRenderAsNormal)) *
          (ideal_x - bounds->x())));
}

float Tab::GetThrobValue() const {
  const bool is_selected = IsSelected();
  double val = is_selected ? kSelectedTabOpacity : 0;

  // Wrapping in closure to only compute offset when needed (animate or hover).
  const auto offset = [=] {
    constexpr float kSelectedTabThrobScale = 0.95f - kSelectedTabOpacity;
    const float opacity = GetHoverOpacity();
    return is_selected ? (kSelectedTabThrobScale * opacity) : opacity;
  };

  if (pulse_animation_.is_animating())
    val += pulse_animation_.GetCurrentValue() * offset();
  else if (hover_controller_.ShouldDraw())
    val += hover_controller_.GetAnimationValue() * offset();

  return val;
}

void Tab::UpdateIconVisibility() {
  // TODO(pkasting): This whole function should go away, and we should simply
  // compute child visibility state in Layout().

  // Don't adjust whether we're centering the favicon or adding extra padding
  // during tab closure; let it stay however it was prior to closing the tab.
  // This prevents the icon and text from sliding left at the end of closing
  // a non-narrow tab.
  if (!closing_) {
    center_icon_ = false;
    extra_padding_before_content_ = false;
  }

  showing_icon_ = showing_alert_indicator_ = false;
  extra_alert_indicator_padding_ = false;

  if (height() < GetLayoutConstant(TAB_HEIGHT))
    return;

  const bool has_favicon = data().show_icon;
  const bool has_alert_icon =
      (alert_indicator_ ? alert_indicator_->showing_alert_state()
                        : data().alert_state) != TabAlertState::NONE;

  if (data().pinned) {
    // When the tab is pinned, we can show one of the two icons; the alert icon
    // is given priority over the favicon. The close buton is never shown.
    showing_alert_indicator_ = has_alert_icon;
    showing_icon_ = has_favicon && !has_alert_icon;
    showing_close_button_ = false;
    return;
  }

  int available_width = GetContentsBounds().width();

  const bool touch_ui = MD::touch_ui();
  const int favicon_width = gfx::kFaviconSize;
  const int alert_icon_width = alert_indicator_->GetPreferredSize().width();
  // In case of touch optimized UI, the close button has an extra padding on the
  // left that needs to be considered.
  const int close_button_width =
      close_button_->GetPreferredSize().width() -
      (touch_ui ? close_button_->GetInsets().right()
                : close_button_->GetInsets().width());
  const bool large_enough_for_close_button =
      available_width >= (touch_ui ? kTouchMinimumContentsWidthForCloseButtons
                                   : kMinimumContentsWidthForCloseButtons);

  showing_close_button_ = !controller_->ShouldHideCloseButtonForTab(this);
  if (IsActive()) {
    // Close button is shown on active tabs regardless of the size.
    if (showing_close_button_)
      available_width -= close_button_width;

    showing_alert_indicator_ =
        has_alert_icon && alert_icon_width <= available_width;
    if (showing_alert_indicator_)
      available_width -= alert_icon_width;

    showing_icon_ = has_favicon && favicon_width <= available_width;
    if (showing_icon_)
      available_width -= favicon_width;
  } else {
    showing_alert_indicator_ =
        has_alert_icon && alert_icon_width <= available_width;
    if (showing_alert_indicator_)
      available_width -= alert_icon_width;

    showing_icon_ = has_favicon && favicon_width <= available_width;
    if (showing_icon_)
      available_width -= favicon_width;

    // Show the close button if it's allowed to show on hover, even if it's
    // forced to be hidden normally.
    const bool show_on_hover = controller_->ShouldShowCloseButtonOnHover();
    showing_close_button_ |= show_on_hover && hover_controller_.ShouldDraw();
    showing_close_button_ &= large_enough_for_close_button;
    if (showing_close_button_ || show_on_hover)
      available_width -= close_button_width;

    // If no other controls are visible, show the alert icon or the favicon
    // even though we don't have enough space. We'll clip the icon in
    // PaintChildren().
    if (!showing_close_button_ && !showing_alert_indicator_ && !showing_icon_) {
      showing_alert_indicator_ = has_alert_icon;
      showing_icon_ = !showing_alert_indicator_ && has_favicon;

      // See comments near top of function on why this conditional is here.
      if (!closing_)
        center_icon_ = true;
    }
  }

  // Don't update padding while the tab is closing, to avoid glitchy-looking
  // behaviour when the close animation causes the tab to get very small
  if (!closing_) {
    // The extra padding is intended to visually balance the close button, so
    // only include it when the close button is shown or will be shown on hover.
    // We also check this for active tabs so that the extra padding doesn't pop
    // in and out as you switch tabs.
    extra_padding_before_content_ = large_enough_for_close_button;
  }

  extra_alert_indicator_padding_ = showing_alert_indicator_ &&
                                   showing_close_button_ &&
                                   large_enough_for_close_button;
}

bool Tab::ShouldRenderAsNormalTab() const {
  return !data().pinned || (width() >= (TabStyle::GetPinnedWidth() +
                                        kPinnedTabExtraWidthToRenderAsNormal));
}

float Tab::GetHoverOpacity() const {
  // Opacity boost varies on tab width.  The interpolation is nonlinear so
  // that most tabs will fall on the low end of the opacity range, but very
  // narrow tabs will still stand out on the high end.
  const float range_start = float{TabStyle::GetStandardWidth()};
  const float range_end = float{TabStyle::GetMinimumInactiveWidth()};
  const float value_in_range = float{width()};
  const float t = (value_in_range - range_start) / (range_end - range_start);
  return controller_->GetHoverOpacityForTab(t * t);
}

void Tab::UpdateTabIconNeedsAttentionBlocked() {
  // Only show the blocked attention indicator on non-active tabs. For active
  // tabs, the user sees the dialog blocking the tab, so there's no point to it
  // and it would be distracting.
  if (IsActive()) {
    icon_->SetAttention(TabIcon::AttentionType::kBlockedWebContents, false);
  } else {
    icon_->SetAttention(TabIcon::AttentionType::kBlockedWebContents,
                        data_.blocked);
  }
}

void Tab::UpdateForegroundColors() {
  // The theme provider may be null if we're not currently in a widget
  // hierarchy.
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;

  // These ratios are calculated from the default Chrome theme colors.
  // Active/inactive are the contrast ratios of the close X against the tab
  // background. Hovered/pressed are the contrast ratios of the highlight circle
  // against the tab background.
  constexpr float kMinimumActiveContrastRatio = 6.05f;
  constexpr float kMinimumInactiveContrastRatio = 4.61f;
  constexpr float kMinimumHoveredContrastRatio = 5.02f;
  constexpr float kMinimumPressedContrastRatio = 4.41f;

  // In some cases, inactive tabs may have background more like active tabs than
  // inactive tabs, so colors should be adapted to ensure appropriate contrast.
  // In particular, text should have plenty of contrast in all cases, so switch
  // to using foreground color designed for active tabs if the tab looks more
  // like an active tab than an inactive tab.
  float expected_opacity = 0.0f;
  if (IsActive()) {
    expected_opacity = 1.0f;
  } else if (IsSelected()) {
    expected_opacity = kSelectedTabOpacity;
  } else if (mouse_hovered_) {
    expected_opacity = GetHoverOpacity();
  }
  SkColor tab_bg_color = color_utils::AlphaBlend(
      controller_->GetTabBackgroundColor(TAB_ACTIVE),
      controller_->GetTabBackgroundColor(TAB_INACTIVE),
      gfx::ToRoundedInt(expected_opacity * SK_AlphaOPAQUE));
  SkColor tab_title_color = controller_->GetTabForegroundColor(
      expected_opacity > 0.5f ? TAB_ACTIVE : TAB_INACTIVE);
  tab_title_color =
      color_utils::GetColorWithMinimumContrast(tab_title_color, tab_bg_color);

  title_->SetEnabledColor(tab_title_color);

  const SkColor base_hovered_color = theme_provider->GetColor(
      ThemeProperties::COLOR_TAB_CLOSE_BUTTON_BACKGROUND_HOVER);
  const SkColor base_pressed_color = theme_provider->GetColor(
      ThemeProperties::COLOR_TAB_CLOSE_BUTTON_BACKGROUND_PRESSED);

  const auto get_color_for_contrast_ratio = [](SkColor fg_color,
                                               SkColor bg_color,
                                               float contrast_ratio) {
    const SkAlpha blend_alpha = color_utils::GetBlendValueWithMinimumContrast(
        bg_color, fg_color, bg_color, contrast_ratio);
    return color_utils::AlphaBlend(fg_color, bg_color, blend_alpha);
  };

  const SkColor generated_icon_color = get_color_for_contrast_ratio(
      tab_title_color, tab_bg_color,
      IsActive() ? kMinimumActiveContrastRatio : kMinimumInactiveContrastRatio);
  const SkColor generated_hovered_color = get_color_for_contrast_ratio(
      base_hovered_color, tab_bg_color, kMinimumHoveredContrastRatio);
  const SkColor generated_pressed_color = get_color_for_contrast_ratio(
      base_pressed_color, tab_bg_color, kMinimumPressedContrastRatio);

  const SkColor generated_hovered_icon_color =
      color_utils::GetColorWithMinimumContrast(tab_title_color,
                                               generated_hovered_color);
  const SkColor generated_pressed_icon_color =
      color_utils::GetColorWithMinimumContrast(tab_title_color,
                                               generated_pressed_color);
  close_button_->SetIconColors(
      generated_icon_color, generated_hovered_icon_color,
      generated_pressed_icon_color, generated_hovered_color,
      generated_pressed_color);

  if (button_color_ != generated_icon_color) {
    button_color_ = generated_icon_color;
    alert_indicator_->OnParentTabButtonColorChanged();
  }
}
