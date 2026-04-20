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
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_ids.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/menu/menu_runner.h"

WebUIPinnedToolbarActions::WebUIPinnedToolbarActions(
    WebUIToolbarWebView* webui_toolbar_web_view)
    : webui_toolbar_web_view_(webui_toolbar_web_view),
      model_(PinnedToolbarActionsModel::Get(
          webui_toolbar_web_view->browser_->GetProfile())) {}

WebUIPinnedToolbarActions::~WebUIPinnedToolbarActions() = default;

void WebUIPinnedToolbarActions::Init() {
  model_observation_.Observe(model_);
  model_->MaybeMigrateExistingPinnedStates();
  OnActionsChanged();
}

void WebUIPinnedToolbarActions::OnActionsChanged() {
  std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr> states;
  base::flat_set<actions::ActionId> processed_actions;

  action_subscriptions_.clear();

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
    state->tooltip = item->GetTooltipText();
    state->accessibility_text = item->GetAccessibleName();
    if (auto element_id = webui_toolbar::ActionIdToElementIdentifier(id)) {
      state->element_id = element_id.GetName();
    }
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

  webui_toolbar_web_view_->OnPinnedToolbarActionsStateChanged(
      std::move(states));
}

const std::vector<actions::ActionId>&
WebUIPinnedToolbarActions::PinnedActionIds() const {
  return model_->PinnedActionIds();
}

actions::ActionItem* WebUIPinnedToolbarActions::GetActionItemFor(
    actions::ActionId id) {
  return actions::ActionManager::Get().FindAction(
      id, webui_toolbar_web_view_->browser_->GetActions()->root_action_item());
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
  NOTIMPLEMENTED();
}

ToolbarButton* WebUIPinnedToolbarActions::GetDownloadButton() {
  // TODO(https://crbug.com/474063115): Implement this.
  NOTIMPLEMENTED();
  return nullptr;
}

views::BubbleAnchor WebUIPinnedToolbarActions::GetBubbleAnchor(
    actions::ActionId action_id) {
  if (IsActionPinnedOrPoppedOut(action_id)) {
    // TODO(https://crbug.com/493870881): Add support for cases where the button
    // was very recently pinned or popped out and the WebUI hasn't had a chance
    // to call TrackedElementHandler::TrackedElementVisibilityChanged(), so the
    // code below will return nullptr.
    return views::BubbleAnchor(
        BrowserElements::From(webui_toolbar_web_view_->browser_)
            ->GetElement(
                webui_toolbar::ActionIdToElementIdentifier(action_id)));
  }
  return views::BubbleAnchor();
}

PinnedActionToolbarButton* WebUIPinnedToolbarActions::GetChromeLabsButton() {
  return nullptr;
}

void WebUIPinnedToolbarActions::UpdatePinnedStateAndAnnounce(
    actions::ActionId id,
    bool pin) {
  NOTIMPLEMENTED();
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
            .SetProperty(
                kSidePanelOpenTriggerKey,
                static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                    SidePanelOpenTrigger::kPinnedEntryToolbarButton))
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
      webui_toolbar_web_view_->browser_, action_id);
  active_context_menu_action_ = action_id;

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS,
      base::BindRepeating(&WebUIPinnedToolbarActions::OnActionsChanged,
                          base::Unretained(this)));

  menu_runner_->RunMenuAt(webui_toolbar_web_view_->GetWidget(), nullptr,
                          screen_rect, views::MenuAnchorPosition::kTopLeft,
                          source_type);
  OnActionsChanged();
}
