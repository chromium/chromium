// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"

#include <algorithm>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/notimplemented.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_ids.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_runner.h"

struct WebUIPinnedToolbarActions::PendingAnchorRequest {
  PendingAnchorRequest(actions::ActionId id,
                       base::OnceCallback<void(BubbleAnchorResult)> cb,
                       ui::ElementTracker::Subscription sub);
  ~PendingAnchorRequest();
  const actions::ActionId action_id;
  base::OnceCallback<void(BubbleAnchorResult)> callback;
  const ui::ElementTracker::Subscription subscription;
};

WebUIPinnedToolbarActions::PendingAnchorRequest::PendingAnchorRequest(
    actions::ActionId id,
    base::OnceCallback<void(BubbleAnchorResult)> cb,
    ui::ElementTracker::Subscription sub)
    : action_id(id), callback(std::move(cb)), subscription(std::move(sub)) {}

WebUIPinnedToolbarActions::PendingAnchorRequest::~PendingAnchorRequest() =
    default;

WebUIPinnedToolbarActions::WebUIPinnedToolbarActions(
    WebUIToolbarControlDelegate* delegate)
    : delegate_(delegate),
      model_(PinnedToolbarActionsModel::Get(
          delegate_->GetBrowser()->GetProfile())) {}

WebUIPinnedToolbarActions::~WebUIPinnedToolbarActions() = default;

void WebUIPinnedToolbarActions::Init() {
  model_observation_.Observe(model_);
  model_->MaybeMigrateExistingPinnedStates();
  OnActionsChanged();
}

void WebUIPinnedToolbarActions::OnThemeChanged() {
  OnActionsChanged();
}

void WebUIPinnedToolbarActions::OnActionsChanged() {
  const auto& old_states = delegate_->GetState().pinned_toolbar_actions_state;
  std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr> states;
  base::flat_set<actions::ActionId> processed_actions;

  action_subscriptions_.clear();

  auto& icon_table = delegate_->GetIconTable();

  auto add_state = [&](actions::ActionId id, bool highlighted) {
    // Don't add two copies of one button, e.g. if pinned and popped-out.
    if (processed_actions.contains(id)) {
      return;
    }
    actions::ActionItem* item = GetActionItemFor(id);
    if (!item) {
      return;
    }
    // Need to monitor the action for changes, e.g. to enabled status.
    action_subscriptions_.push_back(item->AddActionChangedCallback(
        base::BindRepeating(&WebUIPinnedToolbarActions::OnActionsChanged,
                            base::Unretained(this))));

    if (!item->GetVisible()) {
      return;
    }
    if (static_cast<actions::ActionPinnableState>(
            item->GetProperty(actions::kActionItemPinnableKey)) ==
            actions::ActionPinnableState::kNotPinnable &&
        IsActionPinned(id)) {
      return;
    }
    auto mojo_id = webui_toolbar::ActionItemToPinnedToolbarAction(item);
    CHECK(mojo_id) << "Unsupported pinned action type " << id;
    auto state = toolbar_ui_api::mojom::PinnedToolbarActionState::New();
    state->action = *mojo_id;
    state->highlighted =
        highlighted || (menu_runner_ && menu_runner_->IsRunning() &&
                        active_context_menu_action_ == id);
    state->enabled = item->GetEnabled();
    state->activated = item->GetProperty(kActionItemUnderlineIndicatorKey);
    state->tooltip = item->GetTooltipText();
    state->accessibility_text = item->GetAccessibleName();
    if (auto element_id = webui_toolbar::ActionIdToElementIdentifier(id)) {
      state->element_id = element_id.GetName();
    }

    ui::ImageModel image_model;
    if (actions::IsActionClass<actions::StatefulImageActionItem>(item)) {
      image_model = static_cast<actions::StatefulImageActionItem*>(item)
                        ->GetStatefulImage();
    } else {
      image_model = item->GetImage();
    }
    toolbar_ui_api::IconHandle previous_icon;
    // Opportunistically try to reuse the icon handle at the same index as
    // `state` will be at. The reuse won't succeed if the pinned actions are
    // changed, and that's acceptable as this is just an opportunistic
    // optimization.
    if (states.size() < old_states.size()) {
      previous_icon = old_states[states.size()]->icon;
    }
    state->icon = icon_table.RegisterImageModelTryReuse(std::move(image_model),
                                                        previous_icon);

    states.push_back(std::move(state));
    processed_actions.insert(id);
  };

  for (actions::ActionId id : model_->PinnedActionIds()) {
    add_state(id,
              /*highlighted=*/std::ranges::contains(popped_out_actions_, id));
  }

  if (!states.empty()) {
    auto state = toolbar_ui_api::mojom::PinnedToolbarActionState::New();
    state->action = toolbar_ui_api::mojom::PinnedToolbarAction::kDivider;
    state->highlighted = false;
    state->enabled = true;
    states.push_back(std::move(state));
  }

  for (actions::ActionId id : popped_out_actions_) {
    add_state(id, /*highlighted=*/true);
  }

  int old_width = GetWidth();
  delegate_->OnPinnedToolbarActionsStateChanged(std::move(states));
  if (old_width != GetWidth()) {
    delegate_->OnPreferredSizeChanged();
  }
}

const std::vector<actions::ActionId>&
WebUIPinnedToolbarActions::PinnedActionIds() const {
  return model_->PinnedActionIds();
}

actions::ActionItem* WebUIPinnedToolbarActions::GetActionItemFor(
    actions::ActionId id) {
  return actions::ActionManager::Get().FindAction(
      id, BrowserActions::From(delegate_->GetBrowser())->root_action_item());
}

bool WebUIPinnedToolbarActions::IsOverflowed(actions::ActionId id) {
  NOTIMPLEMENTED();
  return false;
}

views::View* WebUIPinnedToolbarActions::GetContainerView() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool WebUIPinnedToolbarActions::ShouldAnyButtonsOverflow(
    gfx::Size available_size) const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIPinnedToolbarActions::UpdateActionState(actions::ActionId id,
                                                  bool is_active) {
  ShowActionEphemerallyInToolbar(id, is_active);
}

void WebUIPinnedToolbarActions::ShowActionEphemerallyInToolbar(
    actions::ActionId id,
    bool show) {
  auto it = std::ranges::find(popped_out_actions_, id);
  if (show) {
    if (it == popped_out_actions_.end()) {
      popped_out_actions_.push_back(id);
      OnActionsChanged();
    }
  } else {
    if (it != popped_out_actions_.end()) {
      popped_out_actions_.erase(it);
      OnActionsChanged();
    }
  }
}

bool WebUIPinnedToolbarActions::IsActionPinned(actions::ActionId id) {
  return model_->Contains(id);
}

bool WebUIPinnedToolbarActions::IsActionPoppedOut(actions::ActionId id) {
  return std::ranges::contains(popped_out_actions_, id) && !IsActionPinned(id);
}

bool WebUIPinnedToolbarActions::IsActionPinnedOrPoppedOut(
    actions::ActionId id) {
  return IsActionPinned(id) || IsActionPoppedOut(id);
}

void WebUIPinnedToolbarActions::PostOrQueueActionAfterAnimation(
    base::OnceClosure action) {
  // Wait for all buttons to finish their "sliding in" animation.
  for (const auto& state : delegate_->GetState().pinned_toolbar_actions_state) {
    if (!state->element_id || state->element_id->empty()) {
      continue;
    }
    std::optional<actions::ActionId> action_id =
        webui_toolbar::PinnedToolbarActionToActionId(state->action);
    if (!action_id) {
      continue;
    }
    ui::ElementIdentifier element_id =
        webui_toolbar::ActionIdToElementIdentifier(*action_id);
    if (!element_id) {
      continue;
    }
    ui::TrackedElement* element =
        BrowserElements::From(delegate_->GetBrowser())->GetElement(element_id);
    if (!element) {
      // Element has not registered yet with TrackedElementManager, meaning
      // it's still animating. Wait for it to finish animating.
      auto subscription =
          ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
              element_id,
              BrowserElements::From(delegate_->GetBrowser())->GetContext(),
              base::BindRepeating(&WebUIPinnedToolbarActions::OnElementShown,
                                  base::Unretained(this), *action_id));

      base::OnceCallback<void(BubbleAnchorResult)> adapted_callback =
          base::BindOnce(&WebUIPinnedToolbarActions::RetryPostOrQueueAction,
                         base::Unretained(this), std::move(action));

      pending_anchor_requests_.push_back(std::make_unique<PendingAnchorRequest>(
          *action_id, std::move(adapted_callback), std::move(subscription)));
      return;
    }
  }

  // All buttons are done animating, so invoke callback.
  std::move(action).Run();
}

void WebUIPinnedToolbarActions::RetryPostOrQueueAction(
    base::OnceClosure action,
    BubbleAnchorResult result) {
  // If something went wrong, invoke `action` rather than risk getting into an
  // infinite loop.
  if (!result.has_value()) {
    std::move(action).Run();
    return;
  }
  PostOrQueueActionAfterAnimation(std::move(action));
}

ToolbarButton* WebUIPinnedToolbarActions::GetDownloadButton() {
  // TODO(https://crbug.com/474063115): Implement this.
  NOTIMPLEMENTED();
  return nullptr;
}

views::BubbleAnchor WebUIPinnedToolbarActions::GetBubbleAnchor(
    actions::ActionId action_id) {
  if (IsActionPinnedOrPoppedOut(action_id)) {
    ui::TrackedElement* element =
        BrowserElements::From(delegate_->GetBrowser())
            ->GetElement(webui_toolbar::ActionIdToElementIdentifier(action_id));
    DCHECK(element);
    return views::BubbleAnchor(element);
  }
  return views::BubbleAnchor();
}

void WebUIPinnedToolbarActions::GetBubbleAnchorAsync(
    actions::ActionId action_id,
    base::OnceCallback<void(BubbleAnchorResult)> callback) {
  auto element_id = webui_toolbar::ActionIdToElementIdentifier(action_id);
  if (!element_id || !IsActionPinnedOrPoppedOut(action_id)) {
    std::move(callback).Run(
        base::unexpected(GetAnchorFailureReason::kAnchorNotFound));
    return;
  }

  ui::TrackedElement* element =
      BrowserElements::From(delegate_->GetBrowser())->GetElement(element_id);
  if (element) {
    std::move(callback).Run(views::BubbleAnchor(element));
    return;
  }

  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          element_id,
          BrowserElements::From(delegate_->GetBrowser())->GetContext(),
          base::BindRepeating(&WebUIPinnedToolbarActions::OnElementShown,
                              base::Unretained(this), action_id));

  pending_anchor_requests_.push_back(std::make_unique<PendingAnchorRequest>(
      action_id, std::move(callback), std::move(subscription)));
}

void WebUIPinnedToolbarActions::OnElementShown(actions::ActionId action_id,
                                               ui::TrackedElement* element) {
  std::vector<base::OnceCallback<void(BubbleAnchorResult)>> callbacks_to_run;
  auto it = pending_anchor_requests_.begin();
  while (it != pending_anchor_requests_.end()) {
    if ((*it)->action_id == action_id) {
      callbacks_to_run.push_back(std::move((*it)->callback));
      it = pending_anchor_requests_.erase(it);
    } else {
      ++it;
    }
  }

  for (auto& callback : callbacks_to_run) {
    std::move(callback).Run(views::BubbleAnchor(element));
  }
}

PinnedActionToolbarButton* WebUIPinnedToolbarActions::GetChromeLabsButton() {
  return nullptr;
}

void WebUIPinnedToolbarActions::UpdatePinnedStateAndAnnounce(
    actions::ActionId id,
    bool pin) {
  if (pin == IsActionPinned(id) ||
      !GetActionItemFor(id)->GetProperty(actions::kActionItemPinnableKey)) {
    return;
  }
  delegate_->GetView()->GetViewAccessibility().AnnounceAlert(
      l10n_util::GetStringUTF16(pin ? IDS_TOOLBAR_BUTTON_PINNED
                                    : IDS_TOOLBAR_BUTTON_UNPINNED));
  model_->UpdatePinnedState(id, pin);
}

void WebUIPinnedToolbarActions::MovePinnedAction(actions::ActionId action_id,
                                                 int target_index) {
  model_->MovePinnedAction(action_id, target_index);
}

void WebUIPinnedToolbarActions::MovePinnedActionBy(actions::ActionId action_id,
                                                   int delta) {
  DCHECK(IsActionPinned(action_id));
  const auto& pinned_action_ids = model_->PinnedActionIds();
  auto iter = std::ranges::find(pinned_action_ids, action_id);
  if (iter == pinned_action_ids.end()) {
    return;
  }
  int current_index = std::distance(pinned_action_ids.begin(), iter);
  int target_index = current_index + delta;
  if (target_index >= 0 &&
      target_index < static_cast<int>(pinned_action_ids.size())) {
    model_->MovePinnedAction(action_id, target_index);
  }
}

void WebUIPinnedToolbarActions::Invoke(
    toolbar_ui_api::mojom::PinnedToolbarAction action_id) {
  std::optional<actions::ActionId> id =
      webui_toolbar::PinnedToolbarActionToActionId(action_id);
  if (!id) {
    return;
  }
  if (actions::ActionItem* action = GetActionItemFor(*id)) {
    action->InvokeAction(
        actions::ActionInvocationContext::Builder()
            .SetProperty(kSidePanelOpenTriggerKey,
                         SidePanelOpenTrigger::kPinnedEntryToolbarButton)
            .Build());
  }
}

void WebUIPinnedToolbarActions::HandleContextMenu(
    toolbar_ui_api::mojom::ContextMenuType menu_type,
    const gfx::Rect& screen_rect,
    ui::mojom::MenuSourceType source_type) {
  actions::ActionId action_id;
  switch (menu_type) {
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionNewIncognitoWindow:
      action_id = kActionNewIncognitoWindow;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionShowPasswordsBubbleOrPage:
      action_id = kActionShowPasswordsBubbleOrPage;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionShowPaymentsBubbleOrPage:
      action_id = kActionShowPaymentsBubbleOrPage;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionShowAddressesBubbleOrPage:
      action_id = kActionShowAddressesBubbleOrPage;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowBookmarks:
      action_id = kActionSidePanelShowBookmarks;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowReadingList:
      action_id = kActionSidePanelShowReadingList;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowHistoryCluster:
      action_id = kActionSidePanelShowHistoryCluster;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionShowDownloads:
      action_id = kActionShowDownloads;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionClearBrowsingData:
      action_id = kActionClearBrowsingData;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionPrint:
      action_id = kActionPrint;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowLensOverlayResults:
      action_id = kActionSidePanelShowLensOverlayResults;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionShowTranslate:
      action_id = kActionShowTranslate;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionQrCodeGenerator:
      action_id = kActionQrCodeGenerator;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionRouteMedia:
      action_id = kActionRouteMedia;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowReadAnything:
      action_id = kActionSidePanelShowReadAnything;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionCopyUrl:
      action_id = kActionCopyUrl;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionSendTabToSelf:
      action_id = kActionSendTabToSelf;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionTaskManager:
      action_id = kActionTaskManager;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionDevTools:
      action_id = kActionDevTools;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionTabSearch:
      action_id = kActionTabSearch;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowContextualTasks:
      action_id = kActionSidePanelShowContextualTasks;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionSidePanelShowLens:
      action_id = kActionSidePanelShowLens;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowAboutThisSite:
      action_id = kActionSidePanelShowAboutThisSite;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowCustomizeChrome:
      action_id = kActionSidePanelShowCustomizeChrome;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowShoppingInsights:
      action_id = kActionSidePanelShowShoppingInsights;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowMerchantTrust:
      action_id = kActionSidePanelShowMerchantTrust;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSendSharedTabGroupFeedback:
      action_id = kActionSendSharedTabGroupFeedback;
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowComments:
      action_id = kActionSidePanelShowComments;
      break;
    default:
      NOTREACHED();
  }

  menu_runner_.reset();
  menu_model_ = std::make_unique<PinnedActionToolbarButtonMenuModel>(
      delegate_->GetBrowser(), action_id);
  active_context_menu_action_ = action_id;

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS,
      base::BindRepeating(&WebUIPinnedToolbarActions::OnActionsChanged,
                          base::Unretained(this)));

  menu_runner_->RunMenuAt(delegate_->GetView()->GetWidget(), nullptr,
                          screen_rect, views::MenuAnchorPosition::kTopLeft,
                          source_type);

  OnActionsChanged();
}

int WebUIPinnedToolbarActions::GetWidth() const {
  const int gap = GetLayoutConstant(LayoutConstant::kToolbarIconDefaultMargin);
  int width = 0;
  for (const auto& it : delegate_->GetState().pinned_toolbar_actions_state) {
    if (it->action == toolbar_ui_api::mojom::PinnedToolbarAction::kDivider) {
      // Matches toolbar_divider.css
      width += GetLayoutConstant(LayoutConstant::kToolbarDividerWidth) +
               2 * GetLayoutConstant(LayoutConstant::kToolbarDividerSpacing) -
               2 * gap;
    } else {
      // Matches toolbar_button.css
      width += GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
    }
    // Matches gap from toolbar_action_container.css
    width += gap;
  }
  width -= !!width * gap;  // Remove last gap if there was a last gap.
  return width;
}
