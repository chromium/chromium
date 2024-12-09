// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/tabs/tab.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/win/pen_event_handler_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

using base::UserMetricsAction;
namespace {

// When a non-pinned tab becomes a pinned tab the width of the tab animates. If
// the width of a pinned tab is at least kPinnedTabExtraWidthToRenderAsNormal
// larger than the desired pinned tab width then the tab is rendered as a normal
// tab. This is done to avoid having the title immediately disappear when
// transitioning a tab from normal to pinned tab.
constexpr int kPinnedTabExtraWidthToRenderAsNormal = 30;

// Additional padding of close button to the right of the tab
// indicator when `extra_alert_indicator_padding_` is true.
constexpr int kTabAlertIndicatorCloseButtonPaddingAdjustmentTouchUI = 8;
constexpr int kTabAlertIndicatorCloseButtonPaddingAdjustment = 4;

// When the DiscardRingImprovements feature is enabled, increase the radius of
// the discard ring by this amount if there is enough space.
constexpr int kIncreasedDiscardIndicatorRadiusDp = 2;

bool g_show_hover_card_on_mouse_hover = true;

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
  if (extra_space > 0) {
    ++extra_space;
  }
  return extra_space / 2;
}

class TabStyleHighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit TabStyleHighlightPathGenerator(TabStyleViews* tab_style_views)
      : tab_style_views_(tab_style_views) {}
  TabStyleHighlightPathGenerator(const TabStyleHighlightPathGenerator&) =
      delete;
  TabStyleHighlightPathGenerator& operator=(
      const TabStyleHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return tab_style_views_->GetPath(TabStyle::PathType::kHighlight, 1.0);
  }

 private:
  const raw_ptr<TabStyleViews, AcrossTasksDanglingUntriaged> tab_style_views_;
};

}  // namespace

// Helper class that observes the tab's close button.
class Tab::TabCloseButtonObserver : public views::ViewObserver {
 public:
  explicit TabCloseButtonObserver(Tab* tab,
                                  views::View* close_button,
                                  TabSlotController* controller)
      : tab_(tab), close_button_(close_button), controller_(controller) {
    DCHECK(close_button_);
    tab_close_button_observation_.Observe(close_button_.get());
  }
  TabCloseButtonObserver(const TabCloseButtonObserver&) = delete;
  TabCloseButtonObserver& operator=(const TabCloseButtonObserver&) = delete;

  ~TabCloseButtonObserver() override {
    DCHECK(tab_close_button_observation_.IsObserving());
    tab_close_button_observation_.Reset();
  }

 private:
  void OnViewFocused(views::View* observed_view) override {
    controller_->UpdateHoverCard(
        tab_, TabSlotController::HoverCardUpdateType::kFocus);
  }

  void OnViewBlurred(views::View* observed_view) override {
    // Only hide hover card if not keyboard navigating.
    if (!controller_->IsFocusInTabs()) {
      controller_->UpdateHoverCard(
          nullptr, TabSlotController::HoverCardUpdateType::kFocus);
    }
  }

  base::ScopedObservation<views::View, views::ViewObserver>
      tab_close_button_observation_{this};

  raw_ptr<Tab, DanglingUntriaged> tab_;
  raw_ptr<views::View, DanglingUntriaged> close_button_;
  raw_ptr<TabSlotController, DanglingUntriaged> controller_;
};

// Tab -------------------------------------------------------------------------

// static
void Tab::SetShowHoverCardOnMouseHoverForTesting(bool value) {
  g_show_hover_card_on_mouse_hover = value;
}

Tab::Tab(TabSlotController* controller)
    : controller_(controller),
      title_(new views::Label()),
      title_animation_(this) {
  DCHECK(controller);

  tab_style_views_ = TabStyleViews::CreateForTab(this);

  // So we get don't get enter/exit on children and don't prematurely stop the
  // hover.
  SetNotifyEnterExitOnChild(true);

  SetID(VIEW_ID_TAB);

  // This will cause calls to GetContentsBounds to return only the rectangle
  // inside the tab shape, rather than to its extents.
  SetBorder(views::CreateEmptyBorder(tab_style_views()->GetContentsInsets()));

  title_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_->SetElideBehavior(gfx::FADE_TAIL);
  title_->SetHandlesTooltips(false);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetText(CoreTabHelper::GetDefaultTitle());
  title_->SetBackgroundColor(SK_ColorTRANSPARENT);
  // |title_| paints on top of an opaque region (the tab background) of a
  // non-opaque layer (the tabstrip's layer), which cannot currently be detected
  // by the subpixel-rendering opacity check.
  // TODO(crbug.com/40725997): Improve the check so that this case doen't
  // need a manual suppression by detecting cases where the text is painted onto
  // onto opaque parts of a not-entirely-opaque layer.
  title_->SetSkipSubpixelRenderingOpacityCheck(true);

  AddChildView(title_.get());

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  icon_ = AddChildView(std::make_unique<TabIcon>());

  alert_indicator_button_ =
      AddChildView(std::make_unique<AlertIndicatorButton>(this));

  // Unretained is safe here because this class outlives its close button, and
  // the controller outlives this Tab.
  close_button_ = AddChildView(std::make_unique<TabCloseButton>(
      base::BindRepeating(&Tab::CloseButtonPressed, base::Unretained(this)),
      base::BindRepeating(&TabSlotController::OnMouseEventInTab,
                          base::Unretained(controller_))));
  close_button_->SetHasInkDropActionOnClick(true);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  showing_close_button_ = !controller_->IsLockedForOnTask();
  close_button_->SetVisible(showing_close_button_);
#endif

  tab_close_button_observer_ = std::make_unique<TabCloseButtonObserver>(
      this, close_button_, controller_);

  title_animation_.SetDuration(base::Milliseconds(100));

  // Enable keyboard focus.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  views::FocusRing::Install(this);
  views::HighlightPathGenerator::Install(
      this,
      std::make_unique<TabStyleHighlightPathGenerator>(tab_style_views()));

  SetProperty(views::kElementIdentifierKey, kTabElementId);

  GetViewAccessibility().SetRole(ax::mojom::Role::kTab);
}

Tab::~Tab() {
  // Observer must be unregistered before child views are destroyed.
  tab_close_button_observer_.reset();
  if (controller_->HoverCardIsShowingForTab(this)) {
    controller_->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kTabRemoved);
  }
}

void Tab::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, &title_animation_);
  title_->SetBoundsRect(target_title_bounds_);
}

void Tab::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, &title_animation_);
  title_->SetBoundsRect(gfx::Tween::RectValueBetween(
      gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                 animation->GetCurrentValue()),
      start_title_bounds_, target_title_bounds_));
}

bool Tab::GetHitTestMask(SkPath* mask) const {
  // When the window is maximized we don't want to shave off the edges or top
  // shadow of the tab, such that the user can click anywhere along the top
  // edge of the screen to select a tab. Ditto for immersive fullscreen.
  *mask = tab_style_views()->GetPath(
      TabStyle::PathType::kHitTest,
      GetWidget()->GetCompositor()->device_scale_factor(),
      /* force_active */ false, TabStyle::RenderUnits::kDips);
  return true;
}

void Tab::Layout(PassKey) {
  const gfx::Rect contents_rect = GetContentsBounds();

  const bool was_showing_icon = showing_icon_;
  UpdateIconVisibility();

  const int start = contents_rect.x();

  // The bounds for the favicon will include extra width for the attention
  // indicator, but visually it will be smaller at kFaviconSize wide.
  gfx::Rect favicon_bounds(start, contents_rect.y(), 0, 0);
  if (showing_icon_) {
    if (center_icon_) {
      // When centering the favicon, the favicon is allowed to escape the normal
      // contents rect.
      favicon_bounds.set_x(Center(width(), gfx::kFaviconSize));
    } else {
      MaybeAdjustLeftForPinnedTab(&favicon_bounds, gfx::kFaviconSize);
    }
    icon_->EnlargeDiscardIndicatorRadius(
        controller()->GetInactiveTabWidth() -
                    2 * tab_style()->GetBottomCornerRadius() >=
                gfx::kFaviconSize + 2 * kIncreasedDiscardIndicatorRadiusDp
            ? kIncreasedDiscardIndicatorRadiusDp
            : 0);

    // Add space for insets outside the favicon bounds.
    favicon_bounds.Inset(-icon_->GetInsets());
    favicon_bounds.set_size(icon_->GetPreferredSize());
  }
  icon_->SetBoundsRect(favicon_bounds);
  icon_->SetVisible(showing_icon_);

  const int after_title_padding = GetLayoutConstant(TAB_AFTER_TITLE_PADDING);

  int close_x = contents_rect.right();
  if (showing_close_button_) {
    // The visible size is the button's hover shape size. The actual size
    // includes the border insets for the button.
    const int close_button_visible_size =
        GetLayoutConstant(TAB_CLOSE_BUTTON_SIZE);
    const gfx::Size close_button_actual_size =
        close_button_->GetPreferredSize();

    // The close button is vertically centered in the contents_rect.
    const int top =
        contents_rect.y() +
        Center(contents_rect.height(), close_button_actual_size.height());

    // The visible part of the close button should be placed against the
    // right of the contents rect unless the tab is so small that it would
    // overflow the left side of the contents_rect, in that case it will be
    // placed in the middle of the tab.
    const int visible_left =
        std::max(close_x - close_button_visible_size,
                 Center(width(), close_button_visible_size));

    // Offset the new bounds rect by the extra padding in the close button.
    const int non_visible_left_padding =
        (close_button_actual_size.width() - close_button_visible_size) / 2;

    close_button_->SetBoundsRect(
        {gfx::Point(visible_left - non_visible_left_padding, top),
         close_button_actual_size});
    close_x = visible_left - after_title_padding;
  }
  close_button_->SetVisible(showing_close_button_);

  if (showing_alert_indicator_) {
    int right = contents_rect.right();
    if (showing_close_button_) {
      right = close_x;
      if (extra_alert_indicator_padding_) {
        right -= ui::TouchUiController::Get()->touch_ui()
                     ? kTabAlertIndicatorCloseButtonPaddingAdjustmentTouchUI
                     : kTabAlertIndicatorCloseButtonPaddingAdjustment;
      }
    }
    const gfx::Size image_size = alert_indicator_button_->GetPreferredSize();
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
    alert_indicator_button_->SetBoundsRect(bounds);
  }
  alert_indicator_button_->SetVisible(showing_alert_indicator_);

  // Size the title to fill the remaining width and use all available height.
  bool show_title = ShouldRenderAsNormalTab();
  if (show_title) {
    int title_left = start;
    if (showing_icon_) {
      // When computing the spacing from the favicon, don't count the actual
      // icon view width (which will include extra room for the alert
      // indicator), but rather the normal favicon width which is what it will
      // look like.
      const int after_favicon = favicon_bounds.x() + icon_->GetInsets().left() +
                                gfx::kFaviconSize +
                                GetLayoutConstant(TAB_PRE_TITLE_PADDING);
      title_left = std::max(title_left, after_favicon);
    }
    int title_right = contents_rect.right();
    if (showing_alert_indicator_) {
      title_right = alert_indicator_button_->x() - after_title_padding;
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

  if (auto* focus_ring = views::FocusRing::Get(this); focus_ring) {
    focus_ring->DeprecatedLayoutImmediately();
  }
}

bool Tab::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN && !IsSelected()) {
    controller_->SelectTab(this, event);
    return true;
  }

  constexpr int kModifiedFlag =
#if BUILDFLAG(IS_MAC)
      ui::EF_COMMAND_DOWN;
#else
      ui::EF_CONTROL_DOWN;
#endif

  if (event.type() == ui::EventType::kKeyPressed &&
      (event.flags() & kModifiedFlag)) {
    const bool is_right = event.key_code() == ui::VKEY_RIGHT;
    const bool is_left = event.key_code() == ui::VKEY_LEFT;
    if (is_right || is_left) {
      const bool is_rtl = base::i18n::IsRTL();
      const bool is_next = (is_right && !is_rtl) || (is_left && is_rtl);
      if (event.flags() & ui::EF_SHIFT_DOWN) {
        if (is_next) {
          controller()->MoveTabLast(this);
        } else {
          controller()->MoveTabFirst(this);
        }
      } else if (is_next) {
        controller()->ShiftTabNext(this);
      } else {
        controller()->ShiftTabPrevious(this);
      }
      return true;
    }
  }

  return false;
}

bool Tab::OnKeyReleased(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE && !IsSelected()) {
    controller_->SelectTab(this, event);
    return true;
  }
  return false;
}

namespace {
bool IsSelectionModifierDown(const ui::MouseEvent& event) {
#if BUILDFLAG(IS_MAC)
  return event.IsCommandDown();
#else
  return event.IsControlDown();
#endif
}
}  // namespace

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  controller_->UpdateHoverCard(nullptr,
                               TabSlotController::HoverCardUpdateType::kEvent);
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
      controller_->SelectTab(this, event);
      base::RecordAction(UserMetricsAction("SwitchTab_Click"));
    }
    ui::MouseEvent cloned_event(event_in_parent, parent(),
                                static_cast<View*>(this));

    if (!closing()) {
      controller_->MaybeStartDrag(this, cloned_event, original_selection);
    }
  }
  return true;
}

bool Tab::OnMouseDragged(const ui::MouseEvent& event) {
  // TODO: ensure ignoring return value is ok.
  std::ignore = controller_->ContinueDrag(this, event);
  return true;
}

void Tab::OnMouseReleased(const ui::MouseEvent& event) {
  controller_->OnMouseEventInTab(this, event);

  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  // In some cases, ending the drag will schedule the tab for destruction; if
  // so, bail immediately, since our members are already dead and we shouldn't
  // do anything else except drop the tab where it is.
  if (controller_->EndDrag(END_DRAG_COMPLETE)) {
    return;
  }

  // Close tab on middle click, but only if the button is released over the tab
  // (normal windows behavior is to discard presses of a UI element where the
  // releases happen off the element).
  if (event.IsOnlyMiddleMouseButton()) {
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
      if (closest_tab) {
        controller_->CloseTab(closest_tab, CLOSE_TAB_FROM_MOUSE);
      }
    }
  } else if (event.IsOnlyLeftMouseButton() && !event.IsShiftDown() &&
             !IsSelectionModifierDown(event)) {
    // If the tab was already selected mouse pressed doesn't change the
    // selection. Reset it now to handle the case where multiple tabs were
    // selected.
    controller_->SelectTab(this, event);
  }
}

void Tab::OnMouseCaptureLost() {
  controller_->EndDrag(END_DRAG_CAPTURE_LOST);
}

void Tab::OnMouseMoved(const ui::MouseEvent& event) {
  tab_style_views()->SetHoverLocation(event.location());
  controller_->OnMouseEventInTab(this, event);

  // Linux enter/leave events are sometimes flaky, so we don't want to "miss"
  // an enter event and fail to hover the tab.
  //
  // In Windows, we won't miss the enter event but mouse input is disabled after
  // a touch gesture and we could end up ignoring the enter event. If the user
  // subsequently moves the mouse, we need to then hover the tab.
  //
  // Either way, this is effectively a no-op if the tab is already in a hovered
  // state (crbug.com/1326272).
  MaybeUpdateHoverStatus(event);
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  MaybeUpdateHoverStatus(event);
}

void Tab::MaybeUpdateHoverStatus(const ui::MouseEvent& event) {
  // During system-DnD-based tab dragging we sometimes receive mouse events, but
  // we shouldn't update the hover status during a drag.
  if (mouse_hovered_ || !GetWidget()->IsMouseEventsEnabled() ||
      TabDragController::IsActive()) {
    return;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Move the hit test area for hovering up so that it is not overlapped by tab
  // hover cards when they are shown.
  // TODO(crbug.com/41467565): Once Linux/CrOS widget transparency is solved,
  // remove that case.
  constexpr int kHoverCardOverlap = 6;
  if (event.location().y() >= height() - kHoverCardOverlap) {
    return;
  }
#endif

  mouse_hovered_ = true;
  tab_style_views()->ShowHover(TabStyle::ShowHoverStyle::kSubtle);
  UpdateForegroundColors();
  DeprecatedLayoutImmediately();
  if (g_show_hover_card_on_mouse_hover) {
    controller_->UpdateHoverCard(
        this, TabSlotController::HoverCardUpdateType::kHover);
  }
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  if (!mouse_hovered_) {
    return;
  }
  mouse_hovered_ = false;
  tab_style_views()->HideHover(TabStyle::HideHoverStyle::kGradual);
  UpdateForegroundColors();
  DeprecatedLayoutImmediately();
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  controller_->UpdateHoverCard(nullptr,
                               TabSlotController::HoverCardUpdateType::kEvent);
  switch (event->type()) {
    case ui::EventType::kGestureTapDown: {
      // TAP_DOWN is only dispatched for the first touch point.
      DCHECK_EQ(1, event->details().touch_points());

      // See comment in OnMousePressed() as to why we copy the event.
      ui::GestureEvent event_in_parent(*event, static_cast<View*>(this),
                                       parent());
      ui::ListSelectionModel original_selection;
      original_selection = controller_->GetSelectionModel();
      if (!IsSelected()) {
        controller_->SelectTab(this, *event);
      }
      gfx::Point loc(event->location());
      views::View::ConvertPointToScreen(this, &loc);
      ui::GestureEvent cloned_event(event_in_parent, parent(),
                                    static_cast<View*>(this));

      if (!closing()) {
#if BUILDFLAG(IS_WIN)
        // If the pen is down on the tab, let pen events fall through to the
        // default window handler until the pen is raised. This allows the
        // default window handler to execute drag-drop on the window when it's
        // moved by its tab, e.g., when the window has a single tab or when a
        // tab is being detached.
        const bool is_pen = event->details().primary_pointer_type() ==
                            ui::EventPointerType::kPen;
        if (is_pen) {
          views::UseDefaultHandlerForPenEventsUntilPenUp();
        }
#endif
        controller_->MaybeStartDrag(this, cloned_event, original_selection);
      }
      break;
    }

    default:
      break;
  }
  event->SetHandled();
}

std::u16string Tab::GetTooltipText(const gfx::Point& p) const {
  // Tab hover cards replace tooltips for tabs.
  return std::u16string();
}

void Tab::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  std::u16string name = controller_->GetAccessibleTabName(this);
  if (!name.empty()) {
    node_data->SetNameChecked(name);
  } else {
    // Under some conditions, |GetAccessibleTabName| returns an empty string.
    node_data->SetNameExplicitlyEmpty();
  }
}

gfx::Size Tab::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(tab_style()->GetStandardWidth(),
                   GetLayoutConstant(TAB_HEIGHT));
}

void Tab::PaintChildren(const views::PaintInfo& info) {
  // Clip children based on the tab's fill path.  This has no effect except when
  // the tab is too narrow to completely show even one icon, at which point this
  // serves to clip the favicon.
  ui::ClipRecorder clip_recorder(info.context());
  // The paint recording scale for tabs is consistent along the x and y axis.
  const float paint_recording_scale = info.paint_recording_scale_x();

  const SkPath clip_path = tab_style_views()->GetPath(
      TabStyle::PathType::kInteriorClip, paint_recording_scale);

  clip_recorder.ClipPathWithAntiAliasing(clip_path);
  View::PaintChildren(info);
}

void Tab::OnPaint(gfx::Canvas* canvas) {
  tab_style_views()->PaintTab(canvas);
}

void Tab::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &Tab::UpdateForegroundColors, base::Unretained(this)));
}

void Tab::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

void Tab::OnFocus() {
  View::OnFocus();
  controller_->UpdateHoverCard(this,
                               TabSlotController::HoverCardUpdateType::kFocus);
}

void Tab::OnBlur() {
  View::OnBlur();
  if (!controller_->IsFocusInTabs()) {
    controller_->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kFocus);
  }
}

void Tab::OnThemeChanged() {
  TabSlotView::OnThemeChanged();
  UpdateForegroundColors();
}

TabSlotView::ViewType Tab::GetTabSlotViewType() const {
  return TabSlotView::ViewType::kTab;
}

TabSizeInfo Tab::GetTabSizeInfo() const {
  return {tab_style()->GetPinnedWidth(), tab_style()->GetMinimumActiveWidth(),
          tab_style()->GetMinimumInactiveWidth(),
          tab_style()->GetStandardWidth()};
}

void Tab::SetClosing(bool closing) {
  closing_ = closing;
  ActiveStateChanged();

  if (closing && views::FocusRing::Get(this)) {
    // When closing, sometimes DCHECK fails because
    // cc::Layer::IsPropertyChangeAllowed() returns false. Deleting
    // the focus ring fixes this. TODO(collinbaker): investigate why
    // this happens.
    views::FocusRing::Remove(this);
  }
}

std::optional<SkColor> Tab::GetGroupColor() const {
  if (closing_ || !group().has_value()) {
    return std::nullopt;
  }

  return controller_->GetPaintedGroupColor(
      controller_->GetGroupColorId(group().value()));
}

ui::ColorId Tab::GetAlertIndicatorColor(TabAlertState state) const {
  const ui::ColorProvider* color_provider = GetColorProvider();
  if (!color_provider) {
    return gfx::kPlaceholderColor;
  }

  int group;
  switch (state) {
    case TabAlertState::MEDIA_RECORDING:
    case TabAlertState::AUDIO_RECORDING:
    case TabAlertState::VIDEO_RECORDING:
    case TabAlertState::DESKTOP_CAPTURING:
      group = 0;
      break;
    case TabAlertState::TAB_CAPTURING:
    case TabAlertState::PIP_PLAYING:
      group = 1;
      break;
    case TabAlertState::AUDIO_PLAYING:
    case TabAlertState::AUDIO_MUTING:
    case TabAlertState::BLUETOOTH_CONNECTED:
    case TabAlertState::BLUETOOTH_SCAN_ACTIVE:
    case TabAlertState::USB_CONNECTED:
    case TabAlertState::HID_CONNECTED:
    case TabAlertState::SERIAL_CONNECTED:
    case TabAlertState::VR_PRESENTING_IN_HEADSET:
      group = 2;
      break;
  }

  const ui::ColorId color_ids[3][2][2] = {
      {{kColorTabAlertMediaRecordingInactiveFrameInactive,
        kColorTabAlertMediaRecordingInactiveFrameActive},
       {kColorTabAlertMediaRecordingActiveFrameInactive,
        kColorTabAlertMediaRecordingActiveFrameActive}},
      {{kColorTabAlertPipPlayingInactiveFrameInactive,
        kColorTabAlertPipPlayingInactiveFrameActive},
       {kColorTabAlertPipPlayingActiveFrameInactive,
        kColorTabAlertPipPlayingActiveFrameActive}},
      {{kColorTabAlertAudioPlayingInactiveFrameInactive,
        kColorTabAlertAudioPlayingInactiveFrameActive},
       {kColorTabAlertAudioPlayingActiveFrameInactive,
        kColorTabAlertAudioPlayingActiveFrameActive}}};
  return color_ids[group][tab_style_views()->GetApparentActiveState() ==
                          TabActive::kActive]
                  [GetWidget()->ShouldPaintAsActive()];
}

bool Tab::IsActive() const {
  return controller_->IsActiveTab(this);
}

void Tab::ActiveStateChanged() {
  UpdateTabIconNeedsAttentionBlocked();
  UpdateForegroundColors();
  icon_->SetActiveState(IsActive());
  alert_indicator_button_->OnParentTabButtonColorChanged();
  DeprecatedLayoutImmediately();
}

void Tab::AlertStateChanged() {
  if (controller_->HoverCardIsShowingForTab(this)) {
    controller_->UpdateHoverCard(
        this, TabSlotController::HoverCardUpdateType::kTabDataChanged);
  }
  DeprecatedLayoutImmediately();
}

void Tab::SelectedStateChanged() {
  UpdateForegroundColors();
  GetViewAccessibility().SetIsSelected(IsSelected());
}

bool Tab::IsSelected() const {
  return controller_->IsTabSelected(this);
}

bool Tab::IsDiscarded() const {
  return data().is_tab_discarded;
}

bool Tab::HasThumbnail() const {
  return data().thumbnail && data().thumbnail->has_data();
}

void Tab::SetData(TabRendererData data) {
  DCHECK(GetWidget());

  if (data_ == data) {
    return;
  }

  TabRendererData old(std::move(data_));
  data_ = std::move(data);

  icon_->SetData(data_);
  icon_->SetCanPaintToLayer(controller_->CanPaintThrobberToLayer());
  UpdateTabIconNeedsAttentionBlocked();

  std::u16string title = data_.title;
  if (title.empty() && !data_.should_render_empty_title) {
    title = icon_->GetShowingLoadingAnimation()
                ? l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE)
                : CoreTabHelper::GetDefaultTitle();
  } else {
    title = Browser::FormatTitleForDisplay(title);
  }
  title_->SetText(title);

  const auto new_alert_state = GetAlertStateToShow(data_.alert_state);
  const auto old_alert_state = GetAlertStateToShow(old.alert_state);
  if (new_alert_state != old_alert_state) {
    alert_indicator_button_->TransitionToAlertState(new_alert_state);
  }
  if (old.pinned != data_.pinned) {
    showing_alert_indicator_ = false;
  }
  if (!data_.pinned && old.pinned) {
    is_animating_from_pinned_ = true;
    // We must set this to true early, because we don't want to set
    // |is_animating_from_pinned_| to false if we lay out before the animation
    // begins.
    set_animating(true);
  }

  if (new_alert_state != old_alert_state || data_.title != old.title) {
    TooltipTextChanged();
  }

  DeprecatedLayoutImmediately();
  SchedulePaint();
}

void Tab::StepLoadingAnimation(const base::TimeDelta& elapsed_time) {
  icon_->StepLoadingAnimation(elapsed_time);

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

void Tab::SetTabNeedsAttention(bool attention) {
  icon_->SetAttention(TabIcon::AttentionType::kTabWantsAttentionStatus,
                      attention);
  SchedulePaint();
}

void Tab::CreateFreezingVote(content::WebContents* contents) {
  if (!freezing_vote_.has_value()) {
    freezing_vote_.emplace(contents);
  }
}

void Tab::ReleaseFreezingVote() {
  freezing_vote_.reset();
}

// static
std::u16string Tab::GetTooltipText(const std::u16string& title,
                                   std::optional<TabAlertState> alert_state) {
  if (!alert_state) {
    return title;
  }

  std::u16string result = title;
  if (!result.empty()) {
    result.append(1, '\n');
  }
  result.append(GetTabAlertStateText(alert_state.value()));
  return result;
}

// static
std::optional<TabAlertState> Tab::GetAlertStateToShow(
    const std::vector<TabAlertState>& alert_states) {
  if (alert_states.empty()) {
    return std::nullopt;
  }

  return alert_states[0];
}

void Tab::SetShouldShowDiscardIndicator(bool enabled) {
  icon_->SetShouldShowDiscardIndicator(enabled);
}

void Tab::MaybeAdjustLeftForPinnedTab(gfx::Rect* bounds,
                                      int visual_width) const {
  if (ShouldRenderAsNormalTab()) {
    return;
  }
  const int pinned_width = tab_style()->GetPinnedWidth();
  const int ideal_delta = width() - pinned_width;
  const int ideal_x = (pinned_width - visual_width) / 2;
  // TODO(crbug.com/40436434): This code is broken when the current width is
  // less than the pinned width.
  bounds->set_x(
      bounds->x() +
      base::ClampRound(
          (1 - static_cast<float>(ideal_delta) /
                   static_cast<float>(kPinnedTabExtraWidthToRenderAsNormal)) *
          (ideal_x - bounds->x())));
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
  }

  showing_icon_ = showing_alert_indicator_ = false;
  extra_alert_indicator_padding_ = false;

  if (height() < GetLayoutConstant(TAB_HEIGHT)) {
    return;
  }

  const bool has_favicon = data().show_icon;
  const bool has_alert_icon =
      (alert_indicator_button_ ? alert_indicator_button_->showing_alert_state()
                               : GetAlertStateToShow(data().alert_state))
          .has_value();

  is_animating_from_pinned_ &= animating();

  if (data().pinned || is_animating_from_pinned_) {
    // When the tab is pinned, we can show one of the two icons; the alert icon
    // is given priority over the favicon. The close buton is never shown.
    showing_alert_indicator_ = has_alert_icon;
    showing_icon_ = has_favicon && !has_alert_icon;
    showing_close_button_ = false;

    // While animating to or from the pinned state, pinned tabs are rendered as
    // normal tabs. Force the extra padding on so the favicon doesn't jitter
    // left and then back right again as it resizes through layout regimes.
    extra_alert_indicator_padding_ = true;
    return;
  }

  int available_width = GetContentsBounds().width();

  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  const int favicon_width = gfx::kFaviconSize;
  const int alert_icon_width =
      alert_indicator_button_->GetPreferredSize().width();
  // In case of touch optimized UI, the close button has an extra padding on the
  // left that needs to be considered.
  const int close_button_width = GetLayoutConstant(TAB_CLOSE_BUTTON_SIZE) +
                                 GetLayoutConstant(TAB_AFTER_TITLE_PADDING);
  const bool large_enough_for_close_button =
      available_width >= (touch_ui ? kTouchMinimumContentsWidthForCloseButtons
                                   : kMinimumContentsWidthForCloseButtons);

  if (IsActive()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Hide tab close button for OnTask if locked. Only applicable for non-web
    // browser scenarios.
    showing_close_button_ = !controller_->IsLockedForOnTask();
#else
    // Close button is shown on active tabs regardless of the size.
    showing_close_button_ = true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    available_width -= close_button_width;

    showing_alert_indicator_ =
        has_alert_icon && alert_icon_width <= available_width;
    if (showing_alert_indicator_) {
      available_width -= alert_icon_width;
    }

    showing_icon_ = has_favicon && favicon_width <= available_width;
    if (showing_icon_) {
      available_width -= favicon_width;
    }
  } else {
    showing_alert_indicator_ =
        has_alert_icon && alert_icon_width <= available_width;
    if (showing_alert_indicator_) {
      available_width -= alert_icon_width;
    }

    showing_icon_ = has_favicon && favicon_width <= available_width;
    if (showing_icon_) {
      available_width -= favicon_width;
    }

    showing_close_button_ =
#if BUILDFLAG(IS_CHROMEOS_ASH)
        !controller_->IsLockedForOnTask() &&
#endif
        large_enough_for_close_button;
    if (showing_close_button_) {
      available_width -= close_button_width;
    }

    // If no other controls are visible, show the alert icon or the favicon
    // even though we don't have enough space. We'll clip the icon in
    // PaintChildren().
    if (!showing_close_button_ && !showing_alert_indicator_ && !showing_icon_) {
      showing_alert_indicator_ = has_alert_icon;
      showing_icon_ = !showing_alert_indicator_ && has_favicon;

      // See comments near top of function on why this conditional is here.
      if (!closing_) {
        center_icon_ = true;
      }
    }
  }

  extra_alert_indicator_padding_ = showing_alert_indicator_ &&
                                   showing_close_button_ &&
                                   large_enough_for_close_button;
}

bool Tab::ShouldRenderAsNormalTab() const {
  return !data().pinned || (width() >= (tab_style()->GetPinnedWidth() +
                                        kPinnedTabExtraWidthToRenderAsNormal));
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

int Tab::GetWidthOfLargestSelectableRegion() const {
  // Assume the entire region to the left of the alert indicator and/or close
  // buttons is available for click-to-select.  If neither are visible, the
  // entire tab region is available.
  const int indicator_left = alert_indicator_button_->GetVisible()
                                 ? alert_indicator_button_->x()
                                 : width();
  const int close_button_left =
      close_button_->GetVisible() ? close_button_->x() : width();
  return std::min(indicator_left, close_button_left);
}

void Tab::UpdateForegroundColors() {
  TabStyle::TabColors colors = tab_style_views()->CalculateTargetColors();
  title_->SetEnabledColor(colors.foreground_color);
  close_button_->SetColors(colors);
  alert_indicator_button_->OnParentTabButtonColorChanged();
  // There may be no focus ring when the tab is closing.
  if (auto* focus_ring = views::FocusRing::Get(this); focus_ring) {
    focus_ring->SetColorId(colors.focus_ring_color);
    focus_ring->SetOutsetFocusRingDisabled(true);
  }
  SchedulePaint();
}

void Tab::CloseButtonPressed(const ui::Event& event) {
  if (!alert_indicator_button_ || !alert_indicator_button_->GetVisible()) {
    base::RecordAction(UserMetricsAction("CloseTab_NoAlertIndicator"));
  } else if (GetAlertStateToShow(data_.alert_state) ==
             TabAlertState::AUDIO_PLAYING) {
    base::RecordAction(UserMetricsAction("CloseTab_AudioIndicator"));
  } else {
    base::RecordAction(UserMetricsAction("CloseTab_RecordingIndicator"));
  }

  const bool from_mouse = event.type() == ui::EventType::kMouseReleased &&
                          !(event.flags() & ui::EF_FROM_TOUCH);
  controller_->CloseTab(
      this, from_mouse ? CLOSE_TAB_FROM_MOUSE : CLOSE_TAB_FROM_TOUCH);
}

BEGIN_METADATA(Tab)
END_METADATA
