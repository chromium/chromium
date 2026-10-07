// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_action_test_accessor.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_ids.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"

PinnedActionTestAccessor::PinnedActionTestAccessor(
    BrowserWindowInterface* browser,
    actions::ActionId action_id)
    : browser_(browser), action_id_(action_id) {}

PinnedActionTestAccessor::~PinnedActionTestAccessor() = default;

// static
PinnedToolbarActions* PinnedActionTestAccessor::GetPinnedToolbarActions(
    BrowserWindowInterface* browser) {
  if (!browser) {
    return nullptr;
  }
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider()) {
    return nullptr;
  }
  return browser_view->toolbar_button_provider()->GetPinnedToolbarActions();
}

PinnedToolbarActions* PinnedActionTestAccessor::GetPinnedToolbarActions()
    const {
  return GetPinnedToolbarActions(browser_);
}

actions::ActionItem* PinnedActionTestAccessor::GetActionItem() const {
  if (auto* pinned_actions = GetPinnedToolbarActions()) {
    return pinned_actions->GetActionItemFor(action_id_);
  }
  return nullptr;
}

// static
PinnedToolbarActionsContainer* PinnedActionTestAccessor::GetViewsContainer(
    BrowserWindowInterface* browser) {
  auto* pinned_actions = GetPinnedToolbarActions(browser);
  if (!pinned_actions) {
    return nullptr;
  }
  return views::AsViewClass<PinnedToolbarActionsContainer>(
      pinned_actions->GetContainerView());
}

PinnedToolbarActionsContainer* PinnedActionTestAccessor::GetViewsContainer()
    const {
  return GetViewsContainer(browser_);
}

PinnedActionToolbarButton* PinnedActionTestAccessor::GetViewsButton() const {
  auto* container = GetViewsContainer();
  if (!container) {
    return nullptr;
  }
  return container->GetButtonFor(action_id_);
}

bool PinnedActionTestAccessor::IsPinned() const {
  if (auto* pinned_actions = GetPinnedToolbarActions()) {
    return pinned_actions->IsActionPinned(action_id_);
  }
  return false;
}

bool PinnedActionTestAccessor::IsPoppedOut() const {
  if (auto* pinned_actions = GetPinnedToolbarActions()) {
    return pinned_actions->IsActionPoppedOut(action_id_);
  }
  return false;
}

bool PinnedActionTestAccessor::IsPinnedOrPoppedOut() const {
  if (auto* pinned_actions = GetPinnedToolbarActions()) {
    return pinned_actions->IsActionPinnedOrPoppedOut(action_id_);
  }
  return false;
}

bool PinnedActionTestAccessor::GetVisible() const {
  if (auto* container = GetViewsContainer()) {
    auto* button = container->GetButtonFor(action_id_);
    return button && button->GetVisible();
  }
  if (!IsPinnedOrPoppedOut()) {
    return false;
  }
  if (auto* item = GetActionItem()) {
    if (!item->GetVisible()) {
      return false;
    }
    if (IsPinned() && static_cast<actions::ActionPinnableState>(
                          item->GetProperty(actions::kActionItemPinnableKey)) ==
                          actions::ActionPinnableState::kNotPinnable) {
      return false;
    }
    return true;
  }
  return false;
}

bool PinnedActionTestAccessor::GetEnabled() const {
  if (auto* container = GetViewsContainer()) {
    auto* button = container->GetButtonFor(action_id_);
    return button && button->GetEnabled();
  }
  if (auto* item = GetActionItem()) {
    return item->GetEnabled();
  }
  return false;
}

bool PinnedActionTestAccessor::IsActivated() const {
  if (auto* container = GetViewsContainer()) {
    auto* button = container->GetButtonFor(action_id_);
    if (!button) {
      return false;
    }
    if (auto* indicator = button->GetStatusIndicatorForTesting()) {
      return indicator->GetVisible();
    }
    return false;
  }
  if (auto* item = GetActionItem()) {
    return item->GetProperty(kActionItemUnderlineIndicatorKey);
  }
  return false;
}

bool PinnedActionTestAccessor::IsHighlighted() const {
  if (auto* pinned_actions = GetPinnedToolbarActions()) {
    return pinned_actions->IsActionHighlighted(action_id_);
  }
  return false;
}

ui::ImageModel PinnedActionTestAccessor::GetImageModel() const {
  if (auto* item = GetActionItem()) {
    if (actions::IsActionClass<actions::StatefulImageActionItem>(item)) {
      const auto* stateful_item =
          static_cast<const actions::StatefulImageActionItem*>(item);
      if (!stateful_item->GetStatefulImage().IsEmpty()) {
        return stateful_item->GetStatefulImage();
      }
    }
    return item->GetImage();
  }
  return ui::ImageModel();
}

std::u16string PinnedActionTestAccessor::GetTooltipText() const {
  if (auto* container = GetViewsContainer()) {
    auto* button = container->GetButtonFor(action_id_);
    return button ? button->GetTooltipText() : std::u16string();
  }
  if (auto* item = GetActionItem()) {
    return std::u16string(item->GetTooltipText());
  }
  return std::u16string();
}

std::u16string PinnedActionTestAccessor::GetAccessibleName() const {
  if (auto* container = GetViewsContainer()) {
    auto* button = container->GetButtonFor(action_id_);
    return button ? button->GetAccessibleName() : std::u16string();
  }
  if (auto* item = GetActionItem()) {
    return std::u16string(item->GetAccessibleName());
  }
  return std::u16string();
}

views::BubbleAnchor PinnedActionTestAccessor::GetBubbleAnchor() const {
  if (auto* pinned_actions = GetPinnedToolbarActions()) {
    return pinned_actions->GetBubbleAnchor(action_id_);
  }
  return views::BubbleAnchor();
}

ui::TrackedElement* PinnedActionTestAccessor::GetElement() const {
  if (!browser_) {
    return nullptr;
  }
  if (auto* container = GetViewsContainer()) {
    auto* button = container->GetButtonFor(action_id_);
    return button
               ? views::ElementTrackerViews::GetInstance()->GetElementForView(
                     button, /*assign_temporary_id=*/true)
               : nullptr;
  }
  ui::ElementIdentifier element_id =
      pinned_toolbar_actions::GetElementIdentifierForAction(action_id_);
  if (!element_id) {
    element_id = webui_toolbar::ActionIdToElementIdentifier(action_id_);
  }
  if (!element_id) {
    return nullptr;
  }
  if (auto* browser_elements = BrowserElements::From(browser_)) {
    return browser_elements->GetElement(element_id);
  }
  return nullptr;
}

void PinnedActionTestAccessor::Click() const {
  if (auto* container = GetViewsContainer()) {
    if (auto* button = container->GetButtonFor(action_id_)) {
      views::test::ButtonTestApi(button).NotifyClick(ui::MouseEvent(
          ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
          ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    }
    return;
  }
  if (auto* item = GetActionItem()) {
    item->InvokeAction();
  }
}

// static
std::vector<actions::ActionId> PinnedActionTestAccessor::GetActionIds(
    BrowserWindowInterface* browser) {
  std::vector<actions::ActionId> result;
  if (auto* container = GetViewsContainer(browser)) {
    for (views::View* child : container->children()) {
      if (views::Button::AsButton(child)) {
        result.push_back(
            static_cast<PinnedActionToolbarButton*>(child)->GetActionId());
      }
    }
    return result;
  }
  auto* pinned_actions = GetPinnedToolbarActions(browser);
  if (!pinned_actions) {
    return result;
  }
  auto* webui_actions = static_cast<WebUIPinnedToolbarActions*>(pinned_actions);
  for (const auto& state :
       webui_actions->delegate_->GetState().pinned_toolbar_actions_state) {
    if (auto id = webui_toolbar::PinnedToolbarActionToActionId(state->action)) {
      result.push_back(*id);
    }
  }
  return result;
}

// static
bool PinnedActionTestAccessor::IsDividerVisible(
    BrowserWindowInterface* browser) {
  if (auto* container = GetViewsContainer(browser)) {
    for (views::View* child : container->children()) {
      if (child->GetProperty(views::kElementIdentifierKey) ==
          kPinnedToolbarActionsContainerDividerElementId) {
        return child->GetVisible();
      }
    }
    return false;
  }
  auto* pinned_actions = GetPinnedToolbarActions(browser);
  if (!pinned_actions) {
    return false;
  }
  auto* webui_actions = static_cast<WebUIPinnedToolbarActions*>(pinned_actions);
  return std::ranges::contains(
      webui_actions->delegate_->GetState().pinned_toolbar_actions_state,
      toolbar_ui_api::mojom::PinnedToolbarAction::kDivider,
      &toolbar_ui_api::mojom::PinnedToolbarActionState::action);
}

// static
void PinnedActionTestAccessor::WaitForAnimation(
    BrowserWindowInterface* browser) {
  auto* pinned_actions = GetPinnedToolbarActions(browser);
  if (!pinned_actions) {
    return;
  }
  // Post `PostOrQueueActionAfterAnimation` to the task runner rather than
  // calling it synchronously so that any pending UI tasks (such as dialog close
  // notifications that trigger toolbar state or animation changes) run first.
  // In the Views implementation, `WaitForAnimatingLayoutManager` always yields
  // via `RunLoop::Run()`, whereas `WebUIPinnedToolbarActions` runs its callback
  // synchronously if no buttons are currently animating.
  base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PinnedToolbarActions::PostOrQueueActionAfterAnimation,
                     base::Unretained(pinned_actions), loop.QuitClosure()));
  loop.Run();
}

// static
void PinnedActionTestAccessor::ReduceAnimationDuration(
    BrowserWindowInterface* browser) {
  auto* pinned_actions = GetPinnedToolbarActions(browser);
  if (!pinned_actions) {
    return;
  }
  if (auto* container = views::AsViewClass<PinnedToolbarActionsContainer>(
          pinned_actions->GetContainerView())) {
    views::test::ReduceAnimationDuration(container);
  }
}
