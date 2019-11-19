// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/numerics/ranges.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_bridge.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/extension.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_skia.h"

namespace {

using WeakToolbarActions = std::vector<ToolbarActionViewController*>;

enum DimensionType { WIDTH, HEIGHT };

// Takes a reference vector |reference| of length n, where n is less than or
// equal to the length of |to_sort|, and rearranges |to_sort| so that
// |to_sort|'s first n elements match the n elements of |reference| (the order
// of any remaining elements in |to_sort| is unspecified).
// |equal| is used to compare the elements of |to_sort| and |reference|.
// This allows us to sort a vector to match another vector of two different
// types without needing to construct a more cumbersome comparator class.
// |FunctionType| should equate to (something similar to)
// bool Equal(const Type1&, const Type2&), but we can't enforce this
// because of MSVC compilation limitations.
template <typename Type1, typename Type2, typename FunctionType>
void SortContainer(std::vector<std::unique_ptr<Type1>>* to_sort,
                   const std::vector<Type2>& reference,
                   FunctionType equal) {
  CHECK_GE(to_sort->size(), reference.size())
      << "|to_sort| must contain all elements in |reference|.";
  if (reference.empty())
    return;
  // Run through the each element and compare it to the reference. If something
  // is out of place, find the correct spot for it.
  for (size_t i = 0; i < reference.size() - 1; ++i) {
    if (!equal(to_sort->at(i).get(), reference[i])) {
      // Find the correct index (it's guaranteed to be after our current
      // index, since everything up to this point is correct), and swap.
      size_t j = i + 1;
      while (!equal(to_sort->at(j).get(), reference[i])) {
        ++j;
        DCHECK_LT(j, to_sort->size())
            << "Item in |reference| not found in |to_sort|.";
      }
      std::swap(to_sort->at(i), to_sort->at(j));
    }
  }
}

// How long to wait until showing an extension message bubble.
int g_extension_bubble_appearance_wait_time_in_seconds = 5;

}  // namespace

// static
bool ToolbarActionsBar::disable_animations_for_testing_ = false;

ToolbarActionsBar::PlatformSettings::PlatformSettings()
    : item_spacing(GetLayoutConstant(TOOLBAR_STANDARD_SPACING)),
      icons_per_overflow_menu_row(1) {}

ToolbarActionsBar::ToolbarActionsBar(ToolbarActionsBarDelegate* delegate,
                                     Browser* browser,
                                     ToolbarActionsBar* main_bar)
    : delegate_(delegate),
      browser_(browser),
      model_(ToolbarActionsModel::Get(browser_->profile())),
      main_bar_(main_bar),
      platform_settings_(),
      popup_owner_(nullptr),
      model_observer_(this),
      suppress_layout_(false),
      suppress_animation_(true),
      should_check_extension_bubble_(!main_bar),
      popped_out_action_(nullptr),
      is_popped_out_sticky_(false),
      is_showing_bubble_(false) {
  if (model_)  // |model_| can be null in unittests.
    model_observer_.Add(model_);

  DCHECK(!base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));

  browser_->tab_strip_model()->AddObserver(this);
}

ToolbarActionsBar::~ToolbarActionsBar() {
  // We don't just call DeleteActions() here because it makes assumptions about
  // the order of deletion between the views and the ToolbarActionsBar.
  DCHECK(toolbar_actions_.empty())
      << "Must call DeleteActions() before destruction.";

  // Make sure we don't listen to any more model changes during
  // ToolbarActionsBar destruction.
  model_observer_.RemoveAll();

  for (ToolbarActionsBarObserver& observer : observers_)
    observer.OnToolbarActionsBarDestroyed();
}

// static
void ToolbarActionsBar::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kToolbarIconSurfacingBubbleAcknowledged, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterInt64Pref(prefs::kToolbarIconSurfacingBubbleLastShowTime,
                              0);
}

// static
gfx::Size ToolbarActionsBar::GetIconAreaSize() {
  return gfx::Size(28, 28);
}

gfx::Size ToolbarActionsBar::GetViewSize() const {
  gfx::Rect rect(GetIconAreaSize());
  rect.Inset(-GetIconAreaInsets());
  return rect.size();
}

gfx::Size ToolbarActionsBar::GetFullSize() const {
  // If there are no actions to show (and this isn't an overflow container),
  // then don't show the container at all.
  if (toolbar_actions_.empty() && !in_overflow_mode())
    return gfx::Size();

  int num_icons = GetIconCount();
  int num_rows = 1;

  if (in_overflow_mode()) {
    // In overflow, we always have a preferred size of a full row (even if we
    // don't use it), and always of at least one row. The parent may decide to
    // show us even when empty, e.g. as a drag target for dragging in icons from
    // the main container.
    num_icons = platform_settings_.icons_per_overflow_menu_row;
    const int icon_count = GetEndIndexInBounds() - GetStartIndexInBounds();
    num_rows += (std::max(0, icon_count - 1) / num_icons);
  }

  return gfx::Size(IconCountToWidth(num_icons), IconCountToWidth(num_rows));
}

int ToolbarActionsBar::GetMinimumWidth() const {
  return platform_settings_.item_spacing;
}

int ToolbarActionsBar::GetMaximumWidth() const {
  return IconCountToWidth(toolbar_actions_.size());
}

int ToolbarActionsBar::IconCountToWidth(size_t icons) const {
  if (icons == 0)
    return 0;
  return icons * GetViewSize().width() +
         (icons - 1) * GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
}

size_t ToolbarActionsBar::WidthToIconCountUnclamped(int pixels) const {
  const int element_padding = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  return std::max(
      (pixels + element_padding) / (GetViewSize().width() + element_padding),
      0);
}

size_t ToolbarActionsBar::WidthToIconCount(int pixels) const {
  return std::min(WidthToIconCountUnclamped(pixels), toolbar_actions_.size());
}

size_t ToolbarActionsBar::GetIconCount() const {
  if (!model_)
    return 0;

  int pop_out_modifier = 0;
  // If there is a popped out action, it could affect the number of visible
  // icons - but only if it wouldn't otherwise be visible.
  if (popped_out_action_) {
    size_t popped_out_index = 0;
    for (; popped_out_index < toolbar_actions_.size(); ++popped_out_index) {
      if (toolbar_actions_[popped_out_index].get() == popped_out_action_)
        break;
    }

    pop_out_modifier = popped_out_index >= model_->visible_icon_count() ? 1 : 0;
  }

  // We purposefully do not account for any "popped out" actions in overflow
  // mode. This is because the popup cannot be showing while the overflow menu
  // is open, so there's no concern there. Also, if the user has a popped out
  // action, and immediately opens the overflow menu, we *want* the action there
  // (since it will close the popup, but do so asynchronously, and we don't
  // want to "slide" the action back in.
  size_t visible_icons =
      in_overflow_mode()
          ? toolbar_actions_.size() - model_->visible_icon_count()
          : model_->visible_icon_count() + pop_out_modifier;

#if DCHECK_IS_ON()
  // Good time for some sanity checks: We should never try to display more
  // icons than we have, and we should always have a view per item in the model.
  // (The only exception is if this is in initialization.)
  if (!toolbar_actions_.empty() && !suppress_layout_ &&
      model_->actions_initialized()) {
    DCHECK_LE(visible_icons, toolbar_actions_.size());
    DCHECK_EQ(model_->action_ids().size(), toolbar_actions_.size());
  }
#endif

  return visible_icons;
}

size_t ToolbarActionsBar::GetStartIndexInBounds() const {
  return in_overflow_mode() ? main_bar_->GetEndIndexInBounds() : 0;
}

size_t ToolbarActionsBar::GetEndIndexInBounds() const {
  // The end index for the main bar is however many icons can fit with the given
  // width. We take the width-after-animation here so that we don't have to
  // constantly adjust both this and the overflow as the size changes (the
  // animations are small and fast enough that this doesn't cause problems).
  return in_overflow_mode()
             ? toolbar_actions_.size()
             : WidthToIconCount(delegate_->GetWidth(
                   ToolbarActionsBarDelegate::GET_WIDTH_AFTER_ANIMATION));
}

bool ToolbarActionsBar::NeedsOverflow() const {
  DCHECK(!in_overflow_mode());
  // We need an overflow view if either the end index is less than the number of
  // icons, if a drag is in progress with the redesign turned on (since the
  // user can drag an icon into the app menu), or if there is a non-sticky
  // popped out action (because the action will pop back into overflow when the
  // menu opens).
  return GetEndIndexInBounds() != toolbar_actions_.size() ||
         is_drag_in_progress() ||
         (popped_out_action_ && !is_popped_out_sticky_);
}

gfx::Rect ToolbarActionsBar::GetFrameForIndex(size_t index) const {
  size_t start_index = GetStartIndexInBounds();

  // If the index is for an action that is before range we show (i.e., is for
  // a button that's on the main bar, and this is the overflow), send back an
  // empty rect.
  if (index < start_index)
    return gfx::Rect();

  const size_t relative_index = index - start_index;
  const int icons_per_overflow_row =
      platform_settings().icons_per_overflow_menu_row;
  const size_t row_index =
      in_overflow_mode() ? relative_index / icons_per_overflow_row : 0;
  const size_t index_in_row = in_overflow_mode()
                                  ? relative_index % icons_per_overflow_row
                                  : relative_index;

  const auto size = GetViewSize();
  const int element_padding = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  return gfx::Rect(gfx::Point(index_in_row * (size.width() + element_padding),
                              row_index * (size.height() + element_padding)),
                   size);
}

std::vector<ToolbarActionViewController*> ToolbarActionsBar::GetActions()
    const {
  std::vector<ToolbarActionViewController*> actions;
  for (const auto& action : toolbar_actions_)
    actions.push_back(action.get());

  // If there is an action that should be popped out, and it's not visible by
  // default, make it the final action in the list.
  if (popped_out_action_) {
    size_t index =
        std::find(actions.begin(), actions.end(), popped_out_action_) -
        actions.begin();
    DCHECK_NE(actions.size(), index);
    size_t visible = GetIconCount();
    if (index >= visible) {
      size_t rindex = actions.size() - index - 1;
      std::rotate(actions.rbegin() + rindex, actions.rbegin() + rindex + 1,
                  actions.rend() - visible + 1);
    }
  }

  return actions;
}

void ToolbarActionsBar::CreateActions() {
  CHECK(toolbar_actions_.empty());
  // If the model isn't initialized, wait for it.
  if (!model_ || !model_->actions_initialized())
    return;

  {
    // We don't redraw the view while creating actions.
    base::AutoReset<bool> layout_resetter(&suppress_layout_, true);

    // Get the toolbar actions.
    toolbar_actions_ =
        model_->CreateActions(browser_, GetMainBar(), in_overflow_mode());
    if (!toolbar_actions_.empty())
      ReorderActions();

    for (size_t i = 0; i < toolbar_actions_.size(); ++i)
      delegate_->AddViewForAction(toolbar_actions_[i].get(), i);
  }

  // Once the actions are created, we should animate the changes.
  suppress_animation_ = false;

  // CreateActions() can be called multiple times, so we need to make sure we
  // haven't already shown the bubble.
  // Extension bubbles can also highlight a subset of actions, so don't show the
  // bubble if the toolbar is already highlighting a different set.
  if (should_check_extension_bubble_ && !is_highlighting()) {
    should_check_extension_bubble_ = false;
    // CreateActions() can be called as part of the browser window set up, which
    // we need to let finish before showing the actions.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ToolbarActionsBar::MaybeShowExtensionBubble,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void ToolbarActionsBar::DeleteActions() {
  HideActivePopup();
  delegate_->RemoveAllViews();
  toolbar_actions_.clear();
}

void ToolbarActionsBar::Update() {
  if (toolbar_actions_.empty())
    return;  // Nothing to do.

  {
    // Don't layout until the end.
    base::AutoReset<bool> layout_resetter(&suppress_layout_, true);
    for (const auto& action : toolbar_actions_)
      action->UpdateState();
  }

  ReorderActions();  // Also triggers a draw.
}

bool ToolbarActionsBar::ShowToolbarActionPopup(const std::string& action_id,
                                               bool grant_active_tab) {
  // Don't override another popup, and only show in the active window.
  if (popup_owner() || !browser_->window()->IsActive())
    return false;

  ToolbarActionViewController* action = GetActionForId(action_id);
  return action && action->ExecuteAction(grant_active_tab);
}

void ToolbarActionsBar::SetOverflowRowWidth(int width) {
  DCHECK(in_overflow_mode());
  // This uses the unclamped icon count to allow the in-menu bar to span the
  // menu width.
  platform_settings_.icons_per_overflow_menu_row =
      std::max(WidthToIconCountUnclamped(width), static_cast<size_t>(1));
}

void ToolbarActionsBar::OnResizeComplete(int width) {
  DCHECK(!in_overflow_mode());  // The user can't resize the overflow container.
  size_t resized_count = WidthToIconCount(width);
  // Save off the desired number of visible icons. We do this now instead of
  // at the end of the animation so that even if the browser is shut down
  // while animating, the right value will be restored on next run.
  model_->SetVisibleIconCount(resized_count);
}

void ToolbarActionsBar::OnDragStarted(size_t index_of_dragged_item) {
  if (in_overflow_mode()) {
    main_bar_->OnDragStarted(index_of_dragged_item);
    return;
  }
  DCHECK(!is_drag_in_progress());
  index_of_dragged_item_ = index_of_dragged_item;
}

void ToolbarActionsBar::OnDragEnded() {
  // All drag-and-drop commands should go to the main bar.
  if (in_overflow_mode()) {
    main_bar_->OnDragEnded();
    return;
  }

  DCHECK(is_drag_in_progress());
  index_of_dragged_item_.reset();
  for (ToolbarActionsBarObserver& observer : observers_)
    observer.OnToolbarActionDragDone();
}

void ToolbarActionsBar::OnDragDrop(int dragged_index,
                                   int dropped_index,
                                   DragType drag_type) {
  if (in_overflow_mode()) {
    // All drag-and-drop commands should go to the main bar.
    main_bar_->OnDragDrop(dragged_index, dropped_index, drag_type);
    return;
  }

  int delta = 0;
  if (drag_type == DRAG_TO_OVERFLOW)
    delta = -1;
  else if (drag_type == DRAG_TO_MAIN &&
           dragged_index >= static_cast<int>(model_->visible_icon_count()))
    delta = 1;
  model_->MoveActionIcon(toolbar_actions_[dragged_index]->GetId(),
                         dropped_index);
  if (delta)
    model_->SetVisibleIconCount(model_->visible_icon_count() + delta);
}

const base::Optional<size_t> ToolbarActionsBar::IndexOfDraggedItem() const {
  DCHECK(!in_overflow_mode());
  return index_of_dragged_item_;
}

void ToolbarActionsBar::OnAnimationEnded() {
  // Notify the observers now, since showing a bubble or popup could potentially
  // cause another animation to start.
  for (ToolbarActionsBarObserver& observer : observers_)
    observer.OnToolbarActionsBarAnimationEnded();

  // Check if we were waiting for animation to complete to either show a
  // message bubble, or to show a popup.
  if (pending_bubble_controller_) {
    ShowToolbarActionBubble(std::move(pending_bubble_controller_));
  } else if (!popped_out_closure_.is_null()) {
    popped_out_closure_.Run();
    popped_out_closure_.Reset();
  }
}

void ToolbarActionsBar::OnBubbleClosed() {
  is_showing_bubble_ = false;
}

bool ToolbarActionsBar::IsActionVisibleOnToolbar(
    const ToolbarActionViewController* action) const {
  if (in_overflow_mode())
    return main_bar_->IsActionVisibleOnToolbar(action);

  if (action == popped_out_action_)
    return true;

  size_t visible_icon_count = std::min(toolbar_actions_.size(), GetIconCount());
  for (size_t index = 0; index < visible_icon_count; ++index)
    if (toolbar_actions_[index].get() == action)
      return true;

  return false;
}

void ToolbarActionsBar::PopOutAction(ToolbarActionViewController* controller,
                                     bool is_sticky,
                                     const base::Closure& closure) {
  DCHECK(!in_overflow_mode()) << "Only the main bar can pop out actions.";
  DCHECK(!popped_out_action_) << "Only one action can be popped out at a time!";
  bool needs_redraw = !IsActionVisibleOnToolbar(controller);
  popped_out_action_ = controller;
  is_popped_out_sticky_ = is_sticky;
  if (needs_redraw) {
    // We suppress animation for this draw, because we need the action to get
    // into position immediately, since it's about to show its popup.
    base::AutoReset<bool> layout_resetter(&suppress_animation_, false);
    delegate_->Redraw(true);
  }

  ResizeDelegate(gfx::Tween::LINEAR);
  if (!delegate_->IsAnimating()) {
    // Don't call the closure re-entrantly.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, closure);
  } else {
    popped_out_closure_ = closure;
  }
}

ToolbarActionViewController* ToolbarActionsBar::GetPoppedOutAction() const {
  return popped_out_action_;
}

void ToolbarActionsBar::UndoPopOut() {
  DCHECK(!in_overflow_mode()) << "Only the main bar can pop out actions.";
  DCHECK(popped_out_action_);
  ToolbarActionViewController* controller = popped_out_action_;
  popped_out_action_ = nullptr;
  is_popped_out_sticky_ = false;
  popped_out_closure_.Reset();
  if (!IsActionVisibleOnToolbar(controller))
    delegate_->Redraw(true);
  ResizeDelegate(gfx::Tween::LINEAR);
}

void ToolbarActionsBar::SetPopupOwner(
    ToolbarActionViewController* popup_owner) {
  // We should never be setting a popup owner when one already exists, and
  // never unsetting one when one wasn't set.
  DCHECK((!popup_owner_ && popup_owner) || (popup_owner_ && !popup_owner));
  popup_owner_ = popup_owner;
}

void ToolbarActionsBar::HideActivePopup() {
  if (popup_owner_)
    popup_owner_->HidePopup();
  DCHECK(!popup_owner_);
}

void ToolbarActionsBar::AddObserver(ToolbarActionsBarObserver* observer) {
  observers_.AddObserver(observer);
}

void ToolbarActionsBar::RemoveObserver(ToolbarActionsBarObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ToolbarActionsBar::ShowToolbarActionBubble(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) {
  DCHECK(!in_overflow_mode());
  if (delegate_->IsAnimating()) {
    // If the toolbar is animating, we can't effectively anchor the bubble,
    // so wait until animation stops.
    pending_bubble_controller_ = std::move(bubble);
  } else if (bubble->ShouldShow()) {
    // We check ShouldShow() above since we show the bubble asynchronously, and
    // it might no longer have been valid.

    // If needed, close the overflow menu before showing the bubble.
    ToolbarActionViewController* controller =
        GetActionForId(bubble->GetAnchorActionId());
    bool close_overflow_menu =
        controller && !IsActionVisibleOnToolbar(controller);
    if (close_overflow_menu)
      delegate_->CloseOverflowMenuIfOpen();

    is_showing_bubble_ = true;
    delegate_->ShowToolbarActionBubble(std::move(bubble));
  }
}

void ToolbarActionsBar::ShowToolbarActionBubbleAsync(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ToolbarActionsBar::ShowToolbarActionBubble,
                     weak_ptr_factory_.GetWeakPtr(), std::move(bubble)));
}

bool ToolbarActionsBar::CloseOverflowMenuIfOpen() {
  return delegate_->CloseOverflowMenuIfOpen();
}

void ToolbarActionsBar::MaybeShowExtensionBubble() {
  std::unique_ptr<extensions::ExtensionMessageBubbleController> controller =
      model_->GetExtensionMessageBubbleController(browser_);
  if (!controller)
    return;

  DCHECK(controller->ShouldShow());
  controller->HighlightExtensionsIfNecessary();  // Safe to call multiple times.

  // Not showing the bubble right away (during startup) has a few benefits:
  // We don't have to worry about focus being lost due to the Omnibox (or to
  // other things that want focus at startup). This allows Esc to work to close
  // the bubble and also solves the keyboard accessibility problem that comes
  // with focus being lost (we don't have a good generic mechanism of injecting
  // bubbles into the focus cycle). Another benefit of delaying the show is
  // that fade-in works (the fade-in isn't apparent if the the bubble appears at
  // startup).
  std::unique_ptr<ToolbarActionsBarBubbleDelegate> delegate(
      new ExtensionMessageBubbleBridge(std::move(controller)));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ToolbarActionsBar::ShowToolbarActionBubble,
                     weak_ptr_factory_.GetWeakPtr(), std::move(delegate)),
      base::TimeDelta::FromSeconds(
          g_extension_bubble_appearance_wait_time_in_seconds));
}

ToolbarActionsBar* ToolbarActionsBar::GetMainBar() {
  return main_bar_ ? main_bar_ : this;
}

// static
void ToolbarActionsBar::set_extension_bubble_appearance_wait_time_for_testing(
    int time_in_seconds) {
  g_extension_bubble_appearance_wait_time_in_seconds = time_in_seconds;
}

gfx::Insets ToolbarActionsBar::GetIconAreaInsets() const {
  return GetLayoutInsets(TOOLBAR_ACTION_VIEW);
}

void ToolbarActionsBar::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
  CHECK(model_->actions_initialized());
  CHECK(GetActionForId(action_id) == nullptr)
      << "Asked to add a toolbar action view for an action that already "
         "exists";

  toolbar_actions_.insert(
      toolbar_actions_.begin() + index,
      model_->CreateActionForId(browser_, GetMainBar(), in_overflow_mode(),
                                action_id));
  delegate_->AddViewForAction(toolbar_actions_[index].get(), index);

  // We may need to resize (e.g. to show the new icon). We don't need to check
  // if an extension is upgrading here, because ResizeDelegate() checks to see
  // if the container is already the proper size, and because if the action is
  // newly incognito enabled, even though it's a reload, it's a new extension to
  // this toolbar.
  ResizeDelegate(gfx::Tween::LINEAR);
}

void ToolbarActionsBar::OnToolbarActionLoadFailed() {
  // When an extension is re-uploaded, it is first unloaded from Chrome. At this
  // point, the extension's icon is initially removed from the toolbar, leaving
  // an empty slot in the toolbar. Then the (newer version of the) extension is
  // loaded, and its icon populates the empty slot.
  //
  // If the extension failed to load, then the empty slot should be removed and
  // hence we resize the toolbar.
  ResizeDelegate(gfx::Tween::EASE_OUT);
}

void ToolbarActionsBar::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  auto iter = toolbar_actions_.begin();
  while (iter != toolbar_actions_.end() && (*iter)->GetId() != action_id)
    ++iter;

  if (iter == toolbar_actions_.end())
    return;

  // The action should outlive the UI element (which is owned by the delegate),
  // so we can't delete it just yet. But we should remove it from the list of
  // actions so that any width calculations are correct.
  std::unique_ptr<ToolbarActionViewController> removed_action =
      std::move(*iter);
  toolbar_actions_.erase(iter);

  // If we kill the view before we undo the popout, highlights and pop-ups can
  // get left in weird states, so undo the popout first.
  if (popped_out_action_ == removed_action.get())
    UndoPopOut();
  delegate_->RemoveViewForAction(removed_action.get());
  removed_action.reset();

  // If the extension is being upgraded we don't want the bar to shrink
  // because the icon is just going to get re-added to the same location.
  // There is an exception if this is an off-the-record profile, and the
  // extension is no longer incognito-enabled.
  if (!extensions::ExtensionSystem::Get(browser_->profile())
           ->runtime_data()
           ->IsBeingUpgraded(action_id) ||
      (browser_->profile()->IsOffTheRecord() &&
       !extensions::util::IsIncognitoEnabled(action_id, browser_->profile()))) {
    if (toolbar_actions_.size() > model_->visible_icon_count()) {
      // If we have more icons than we can show, then we must not be changing
      // the container size (since we either removed an icon from the main
      // area and one from the overflow list will have shifted in, or we
      // removed an entry directly from the overflow list).
      delegate_->Redraw(false);
    } else {
      // Either we went from overflow to no-overflow, or we shrunk the no-
      // overflow container by 1.  Either way the size changed, so animate.
      ResizeDelegate(gfx::Tween::EASE_OUT);
    }
  }
}

void ToolbarActionsBar::OnToolbarActionMoved(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
  DCHECK(index >= 0 && index < static_cast<int>(toolbar_actions_.size()));
  // Unfortunately, |index| doesn't really mean a lot to us, because this
  // window's toolbar could be different (if actions are popped out). Just
  // do a full reorder.
  ReorderActions();
}

void ToolbarActionsBar::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  ToolbarActionViewController* action = GetActionForId(action_id);
  // There might not be a view in cases where we are highlighting or if we
  // haven't fully initialized the actions.
  if (action)
    action->UpdateState();
}

void ToolbarActionsBar::OnToolbarVisibleCountChanged() {
  ResizeDelegate(gfx::Tween::EASE_OUT);
}

void ToolbarActionsBar::ResizeDelegate(gfx::Tween::Type tween_type) {
  int desired_width = GetFullSize().width();
  if (desired_width !=
      delegate_->GetWidth(ToolbarActionsBarDelegate::GET_WIDTH_CURRENT)) {
    delegate_->ResizeAndAnimate(tween_type, desired_width);
  } else if (delegate_->IsAnimating()) {
    // It's possible that we're right where we're supposed to be in terms of
    // width, but that we're also currently resizing. If this is the case, end
    // the current animation with the current width.
    delegate_->StopAnimating();
  } else {
    // We may already be at the right size (this can happen frequently with
    // overflow, where we have a fixed width, and in tests, where we skip
    // animations). If this is the case, we still need to Redraw(), because the
    // icons within the toolbar may have changed (e.g. if we removed one
    // action and added a different one in quick succession).
    delegate_->Redraw(false);
  }
}

void ToolbarActionsBar::OnToolbarHighlightModeChanged(bool is_highlighting) {
  if (!model_->actions_initialized())
    return;

  {
    base::AutoReset<bool> layout_resetter(&suppress_layout_, true);
    base::AutoReset<bool> animation_resetter(&suppress_animation_, true);
    std::set<std::string> model_action_ids;
    for (const auto& model_action_id : model_->action_ids()) {
      model_action_ids.insert(model_action_id);

      bool found = false;
      for (size_t i = 0; i < toolbar_actions_.size(); ++i) {
        if (toolbar_actions_[i]->GetId() == model_action_id) {
          found = true;
          break;
        }
      }

      if (!found) {
        toolbar_actions_.push_back(model_->CreateActionForId(
            browser_, GetMainBar(), in_overflow_mode(), model_action_id));
        delegate_->AddViewForAction(toolbar_actions_.back().get(),
                                    toolbar_actions_.size() - 1);
      }
    }

    for (auto iter = toolbar_actions_.begin();
         iter != toolbar_actions_.end();) {
      if (model_action_ids.count((*iter)->GetId()) == 0) {
        delegate_->RemoveViewForAction(iter->get());
        iter = toolbar_actions_.erase(iter);
      } else {
        ++iter;
      }
    }
  }

  ReorderActions();
}

void ToolbarActionsBar::OnToolbarModelInitialized() {
  // We shouldn't have any actions before the model is initialized.
  CHECK(toolbar_actions_.empty());
  CreateActions();
  ResizeDelegate(gfx::Tween::EASE_OUT);
}

void ToolbarActionsBar::OnToolbarPinnedActionsChanged() {
  NOTREACHED();
}

void ToolbarActionsBar::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->empty() || !selection.active_tab_changed())
    return;

  extensions::MaybeShowExtensionControlledNewTabPage(browser_,
                                                     selection.new_contents);
}

void ToolbarActionsBar::ReorderActions() {
  if (toolbar_actions_.empty())
    return;

  // First, reset the order to that of the model.
  auto compare = [](ToolbarActionViewController* const& action,
                    const ToolbarActionsModel::ActionId& action_id) {
    return action->GetId() == action_id;
  };
  SortContainer(&toolbar_actions_, model_->action_ids(), compare);

  // Our visible browser actions may have changed - re-Layout() and check the
  // size (if we aren't suppressing the layout).
  if (!suppress_layout_) {
    ResizeDelegate(gfx::Tween::EASE_OUT);
    delegate_->Redraw(true);
  }
}

ToolbarActionViewController* ToolbarActionsBar::GetActionForId(
    const std::string& action_id) {
  for (const auto& action : toolbar_actions_) {
    if (action->GetId() == action_id)
      return action.get();
  }
  return nullptr;
}

content::WebContents* ToolbarActionsBar::GetCurrentWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
