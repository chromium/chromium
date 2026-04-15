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
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"

namespace {

std::optional<toolbar_ui_api::mojom::PinnedToolbarAction>
ActionIdToPinnedToolbarAction(actions::ActionId action) {
  switch (action) {
    case kActionNewIncognitoWindow:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kNewIncognitoWindow;
    case kActionShowPasswordsBubbleOrPage:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kShowPasswordsBubbleOrPage;
    case kActionShowPaymentsBubbleOrPage:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kShowPaymentsBubbleOrPage;
    case kActionShowAddressesBubbleOrPage:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kShowAddressesBubbleOrPage;
    case kActionSidePanelShowBookmarks:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowBookmarks;
    case kActionSidePanelShowReadingList:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowReadingList;
    case kActionSidePanelShowHistoryCluster:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowHistoryCluster;
    case kActionShowDownloads:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kShowDownloads;
    case kActionClearBrowsingData:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kClearBrowsingData;
    case kActionPrint:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;
    case kActionSidePanelShowLensOverlayResults:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowLensOverlayResults;
    case kActionShowTranslate:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate;
    case kActionQrCodeGenerator:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kQrCodeGenerator;
    case kActionRouteMedia:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMedia;
    case kActionSidePanelShowReadAnything:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowReadAnything;
    case kActionCopyUrl:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kCopyUrl;
    case kActionSendTabToSelf:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kSendTabToSelf;
    case kActionTaskManager:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kTaskManager;
    case kActionDevTools:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kDevTools;
    case kActionTabSearch:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kTabSearch;
    case kActionSidePanelShowContextualTasks:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowContextualTasks;
    case kActionSidePanelShowLens:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowLens;
    case kActionSidePanelShowAboutThisSite:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowAboutThisSite;
    case kActionSidePanelShowCustomizeChrome:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowCustomizeChrome;
    case kActionSidePanelShowShoppingInsights:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowShoppingInsights;
    case kActionSidePanelShowMerchantTrust:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSidePanelShowMerchantTrust;
    case kActionSendSharedTabGroupFeedback:
      return toolbar_ui_api::mojom::PinnedToolbarAction::
          kSendSharedTabGroupFeedback;
    case kActionSidePanelShowComments:
      return toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowComments;
    default:
      return std::nullopt;
  }
}

std::optional<actions::ActionId> PinnedToolbarActionToActionId(
    toolbar_ui_api::mojom::PinnedToolbarAction action) {
  switch (action) {
    case toolbar_ui_api::mojom::PinnedToolbarAction::kUnspecified:
      return std::nullopt;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kNewIncognitoWindow:
      return kActionNewIncognitoWindow;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowPasswordsBubbleOrPage:
      return kActionShowPasswordsBubbleOrPage;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowPaymentsBubbleOrPage:
      return kActionShowPaymentsBubbleOrPage;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowAddressesBubbleOrPage:
      return kActionShowAddressesBubbleOrPage;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowBookmarks:
      return kActionSidePanelShowBookmarks;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowReadingList:
      return kActionSidePanelShowReadingList;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowHistoryCluster:
      return kActionSidePanelShowHistoryCluster;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowDownloads:
      return kActionShowDownloads;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kClearBrowsingData:
      return kActionClearBrowsingData;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kPrint:
      return kActionPrint;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowLensOverlayResults:
      return kActionSidePanelShowLensOverlayResults;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate:
      return kActionShowTranslate;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kQrCodeGenerator:
      return kActionQrCodeGenerator;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMedia:
      return kActionRouteMedia;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowReadAnything:
      return kActionSidePanelShowReadAnything;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kCopyUrl:
      return kActionCopyUrl;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSendTabToSelf:
      return kActionSendTabToSelf;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kTaskManager:
      return kActionTaskManager;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kDevTools:
      return kActionDevTools;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kTabSearch:
      return kActionTabSearch;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowContextualTasks:
      return kActionSidePanelShowContextualTasks;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowLens:
      return kActionSidePanelShowLens;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowAboutThisSite:
      return kActionSidePanelShowAboutThisSite;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowCustomizeChrome:
      return kActionSidePanelShowCustomizeChrome;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowShoppingInsights:
      return kActionSidePanelShowShoppingInsights;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSidePanelShowMerchantTrust:
      return kActionSidePanelShowMerchantTrust;
    case toolbar_ui_api::mojom::PinnedToolbarAction::
        kSendSharedTabGroupFeedback:
      return kActionSendSharedTabGroupFeedback;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowComments:
      return kActionSidePanelShowComments;
    case toolbar_ui_api::mojom::PinnedToolbarAction::kDivider:
      return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace

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
    auto mojo_id = ActionIdToPinnedToolbarAction(id);
    CHECK(mojo_id) << "Unsupported pinned action type " << id;
    auto state = toolbar_ui_api::mojom::PinnedToolbarActionState::New();
    state->action = *mojo_id;
    state->highlighted = highlighted;
    state->enabled = item->GetEnabled();
    state->tooltip = item->GetTooltipText();
    state->accessibility_text = item->GetAccessibleName();
    auto element_id = element_ids_.find(id);
    if (element_id != element_ids_.end()) {
      state->element_id = element_id->second.GetName();
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
  NOTIMPLEMENTED();
  return views::BubbleAnchor();
}

void WebUIPinnedToolbarActions::SetActionElementIdentifier(
    actions::ActionId action_id,
    ui::ElementIdentifier element_id) {
  if (element_id) {
    const auto known_ids = WebUIToolbarUI::GetKnownElementIdentifiers();
    CHECK(std::find(known_ids.begin(), known_ids.end(), element_id) !=
          known_ids.end());
    element_ids_[action_id] = element_id;
  } else {
    element_ids_.erase(action_id);
  }
  OnActionsChanged();
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
      PinnedToolbarActionToActionId(action_id);
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
