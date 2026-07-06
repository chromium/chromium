// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/webui_page_action_control.h"

#include <optional>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/actions/actions.h"

namespace page_actions {

using MojomPageActionId = toolbar_ui_api::mojom::PageActionId;

namespace {

using Code = mojo_base::mojom::Code;
using Error = mojo_base::mojom::Error;

actions::ActionId MojomPageActionIdToActionId(
    MojomPageActionId page_action_id) {
  switch (page_action_id) {
    case MojomPageActionId::kActionAiMode:
      return kActionAiMode;
    case MojomPageActionId::kActionIndigo:
      return kActionIndigo;
    case MojomPageActionId::kActionMultistepFilter:
      return kActionMultistepFilter;
    case MojomPageActionId::kActionSidePanelShowLensOverlayResults:
      return kActionSidePanelShowLensOverlayResults;
    case MojomPageActionId::kActionLensOverlayHomework:
      return kActionLensOverlayHomework;
    case MojomPageActionId::kActionShowTranslate:
      return kActionShowTranslate;
    case MojomPageActionId::kActionShowMemorySaverChip:
      return kActionShowMemorySaverChip;
    case MojomPageActionId::kActionShowJsOptimizationsIcon:
      return kActionShowJsOptimizationsIcon;
    case MojomPageActionId::kActionRecordReplay:
      return kActionRecordReplay;
    case MojomPageActionId::kActionShowIntentPicker:
      return kActionShowIntentPicker;
    case MojomPageActionId::kActionZoomNormal:
      return kActionZoomNormal;
    case MojomPageActionId::kActionSidePanelShowReadAnything:
      return kActionSidePanelShowReadAnything;
    case MojomPageActionId::kActionOffersAndRewardsForPage:
      return kActionOffersAndRewardsForPage;
    case MojomPageActionId::kActionShowFileSystemAccess:
      return kActionShowFileSystemAccess;
    case MojomPageActionId::kActionInstallPwa:
      return kActionInstallPwa;
    case MojomPageActionId::kActionCommercePriceInsights:
      return kActionCommercePriceInsights;
    case MojomPageActionId::kActionCommerceDiscounts:
      return kActionCommerceDiscounts;
    case MojomPageActionId::kActionShowPasswordsBubbleOrPage:
      return kActionShowPasswordsBubbleOrPage;
    case MojomPageActionId::kActionShowCollaborationRecentActivity:
      return kActionShowCollaborationRecentActivity;
    case MojomPageActionId::kActionAutofillMandatoryReauth:
      return kActionAutofillMandatoryReauth;
    case MojomPageActionId::kActionFind:
      return kActionFind;
    case MojomPageActionId::kActionShowCookieControls:
      return kActionShowCookieControls;
    case MojomPageActionId::kActionShowAddressesBubbleOrPage:
      return kActionShowAddressesBubbleOrPage;
    case MojomPageActionId::kActionVirtualCardEnroll:
      return kActionVirtualCardEnroll;
    case MojomPageActionId::kActionFilledCardInformation:
      return kActionFilledCardInformation;
    case MojomPageActionId::kActionShowPaymentsBubbleOrPage:
      return kActionShowPaymentsBubbleOrPage;
    case MojomPageActionId::kActionSidePanelShowContextualTasks:
      return kActionSidePanelShowContextualTasks;
    case MojomPageActionId::kActionBookmarkThisTab:
      return kActionBookmarkThisTab;
    case MojomPageActionId::kActionFederation:
      return kActionFederation;
    case MojomPageActionId::kActionGlicContextualCueing:
      return kActionGlicContextualCueing;
    case MojomPageActionId::kActionAnchoredContextualCue:
      return kActionAnchoredContextualCue;
    case MojomPageActionId::kActionWebAuthnAmbientSignin:
      return kActionWebAuthnAmbientSignin;
    case MojomPageActionId::kActionAutofillPayment:
      return kActionAutofillPayment;
    case MojomPageActionId::kActionShowPaymentsChurnedUsersBubble:
      return kActionShowPaymentsChurnedUsersBubble;
    case MojomPageActionId::kActionFakePageActionForDebug:
      return kActionFakePageActionForDebug;
  }
  NOTREACHED();
}

MojomPageActionId ActionIdToMojomPageActionId(actions::ActionId action_id) {
  switch (action_id) {
    case kActionAiMode:
      return MojomPageActionId::kActionAiMode;
    case kActionIndigo:
      return MojomPageActionId::kActionIndigo;
    case kActionMultistepFilter:
      return MojomPageActionId::kActionMultistepFilter;
    case kActionSidePanelShowLensOverlayResults:
      return MojomPageActionId::kActionSidePanelShowLensOverlayResults;
    case kActionLensOverlayHomework:
      return MojomPageActionId::kActionLensOverlayHomework;
    case kActionShowTranslate:
      return MojomPageActionId::kActionShowTranslate;
    case kActionShowMemorySaverChip:
      return MojomPageActionId::kActionShowMemorySaverChip;
    case kActionShowJsOptimizationsIcon:
      return MojomPageActionId::kActionShowJsOptimizationsIcon;
    case kActionRecordReplay:
      return MojomPageActionId::kActionRecordReplay;
    case kActionShowIntentPicker:
      return MojomPageActionId::kActionShowIntentPicker;
    case kActionZoomNormal:
      return MojomPageActionId::kActionZoomNormal;
    case kActionSidePanelShowReadAnything:
      return MojomPageActionId::kActionSidePanelShowReadAnything;
    case kActionOffersAndRewardsForPage:
      return MojomPageActionId::kActionOffersAndRewardsForPage;
    case kActionShowFileSystemAccess:
      return MojomPageActionId::kActionShowFileSystemAccess;
    case kActionInstallPwa:
      return MojomPageActionId::kActionInstallPwa;
    case kActionCommercePriceInsights:
      return MojomPageActionId::kActionCommercePriceInsights;
    case kActionCommerceDiscounts:
      return MojomPageActionId::kActionCommerceDiscounts;
    case kActionShowPasswordsBubbleOrPage:
      return MojomPageActionId::kActionShowPasswordsBubbleOrPage;
    case kActionShowCollaborationRecentActivity:
      return MojomPageActionId::kActionShowCollaborationRecentActivity;
    case kActionAutofillMandatoryReauth:
      return MojomPageActionId::kActionAutofillMandatoryReauth;
    case kActionFind:
      return MojomPageActionId::kActionFind;
    case kActionShowCookieControls:
      return MojomPageActionId::kActionShowCookieControls;
    case kActionShowAddressesBubbleOrPage:
      return MojomPageActionId::kActionShowAddressesBubbleOrPage;
    case kActionVirtualCardEnroll:
      return MojomPageActionId::kActionVirtualCardEnroll;
    case kActionFilledCardInformation:
      return MojomPageActionId::kActionFilledCardInformation;
    case kActionShowPaymentsBubbleOrPage:
      return MojomPageActionId::kActionShowPaymentsBubbleOrPage;
    case kActionSidePanelShowContextualTasks:
      return MojomPageActionId::kActionSidePanelShowContextualTasks;
    case kActionBookmarkThisTab:
      return MojomPageActionId::kActionBookmarkThisTab;
    case kActionFederation:
      return MojomPageActionId::kActionFederation;
    case kActionGlicContextualCueing:
      return MojomPageActionId::kActionGlicContextualCueing;
    case kActionAnchoredContextualCue:
      return MojomPageActionId::kActionAnchoredContextualCue;
    case kActionWebAuthnAmbientSignin:
      return MojomPageActionId::kActionWebAuthnAmbientSignin;
    case kActionAutofillPayment:
      return MojomPageActionId::kActionAutofillPayment;
    case kActionShowPaymentsChurnedUsersBubble:
      return MojomPageActionId::kActionShowPaymentsChurnedUsersBubble;
    case kActionFakePageActionForDebug:
      return MojomPageActionId::kActionFakePageActionForDebug;
  }
  NOTREACHED();
}

}  // namespace

// Instantiated per ActionId.
class WebUIPageActionControl::WebUIPageActionDelegate
    : public page_actions::PageActionController::Delegate,
      public page_actions::PageActionModelObserver {
 public:
  WebUIPageActionDelegate(actions::ActionId action_id,
                          actions::ActionItem& action_item,
                          WebUIPageActionControl& owner)
      : action_id_(action_id), action_item_(action_item), owner_(owner) {}

  ~WebUIPageActionDelegate() override { observation_.Reset(); }

  void SetController(page_actions::PageActionController* controller);

  // page_actions::PageActionController::Delegate:
  void SetIsChipShowingChangedCallback(
      IsChipShowingChangedCallback callback) override {
    is_chip_showing_changed_callback_ = std::move(callback);
  }
  void SetImageAnimationStartedCallback(
      ImageAnimationStartedCallback callback) override {
    image_animation_started_callback_ = std::move(callback);
  }
  void SetAnchoredMessageCloseCallback(
      base::RepeatingClosure callback) override {
    anchored_message_close_callback_ = std::move(callback);
  }
  void SetAnchoredMessageExpandCallback(
      base::RepeatingClosure callback) override {
    anchored_message_expand_callback_ = std::move(callback);
  }
  void SetAnchoredMessageCollapseCallback(
      base::RepeatingClosure callback) override {
    anchored_message_collapse_callback_ = std::move(callback);
  }
  void SetClickCallback(
      base::RepeatingCallback<void(page_actions::PageActionTrigger)> callback)
      override {
    click_callback_ = std::move(callback);
  }

  // page_actions::PageActionModelObserver:
  void OnPageActionModelChanged(
      const page_actions::PageActionModelInterface& model) override;
  void OnPageActionModelWillBeDeleted(
      const page_actions::PageActionModelInterface& model) override;

  toolbar_ui_api::mojom::PageActionStatePtr GetState() const;
  void NotifyClick(PageActionTrigger trigger);
  void NotifyChipShowingChanged();

 private:
  const actions::ActionId action_id_;
  // Safe because the ActionItem tree is owned by BrowserActions (via
  // BrowserWindowFeatures), which is owned by Browser. The delegate is owned
  // by WebUIPageActionControl -> WebUILocationBar -> WebUIToolbarWebView ->
  // ToolbarView -> BrowserView -> BrowserWindow, which is also owned by
  // Browser. Browser destructor destroys BrowserWindow (and thus this delegate)
  // before BrowserWindowFeatures (and thus the ActionItem tree), ensuring
  // action_item_ outlives this delegate.
  const raw_ref<actions::ActionItem> action_item_;
  // Safe because the owner WebUIPageActionControl owns this delegate (via
  // std::unique_ptr in `delegates_` map), so the owner must outlive this
  // delegate.
  const raw_ref<WebUIPageActionControl> owner_;

  // Pointer to the active tab's controller. Updated when the active tab
  // changes. The controller is owned by TabFeatures (which is owned by TabModel
  // which is owned by TabStripModel). We reset/update this pointer in
  // SetController(). If the model is deleted (e.g. tab closed), we reset this
  // to nullptr in OnPageActionModelWillBeDeleted().
  raw_ptr<page_actions::PageActionController> controller_ = nullptr;
  base::ScopedObservation<page_actions::PageActionModelInterface,
                          page_actions::PageActionModelObserver>
      observation_{this};
  base::CallbackListSubscription action_item_subscription_;

  IsChipShowingChangedCallback is_chip_showing_changed_callback_ =
      base::DoNothing();
  ImageAnimationStartedCallback image_animation_started_callback_ =
      base::DoNothing();
  base::RepeatingClosure anchored_message_close_callback_ = base::DoNothing();
  base::RepeatingClosure anchored_message_expand_callback_ = base::DoNothing();
  base::RepeatingClosure anchored_message_collapse_callback_ =
      base::DoNothing();
  base::RepeatingCallback<void(page_actions::PageActionTrigger)>
      click_callback_ = base::DoNothing();

  // The last state sent to the WebUI. Null if the action was not visible.
  toolbar_ui_api::mojom::PageActionStatePtr old_state_;
};

void WebUIPageActionControl::WebUIPageActionDelegate::SetController(
    page_actions::PageActionController* controller) {
  observation_.Reset();
  action_item_subscription_ = {};
  controller_ = controller;

  if (controller_) {
    controller_->RegisterCallbacks(page_actions::PageActionPassKey(),
                                   action_id_, this);
    controller_->AddObserver(action_id_, observation_);
    action_item_subscription_ =
        controller_->CreateActionItemSubscription(&*action_item_);
    OnPageActionModelChanged(*observation_.GetSource());
  } else {
    if (old_state_) {
      old_state_ = nullptr;
      owner_->NotifyPageActionStateChanged();
    }
  }
}

void WebUIPageActionControl::WebUIPageActionDelegate::OnPageActionModelChanged(
    const page_actions::PageActionModelInterface& model) {
  toolbar_ui_api::mojom::PageActionStatePtr new_state = GetState();

  if (!old_state_.Equals(new_state)) {
    old_state_ = std::move(new_state);
    owner_->NotifyPageActionStateChanged();
  }
}

void WebUIPageActionControl::WebUIPageActionDelegate::
    OnPageActionModelWillBeDeleted(
        const page_actions::PageActionModelInterface& model) {
  observation_.Reset();
  action_item_subscription_ = {};
  controller_ = nullptr;
  if (old_state_) {
    old_state_ = nullptr;
    owner_->NotifyPageActionStateChanged();
  }
}

toolbar_ui_api::mojom::PageActionStatePtr
WebUIPageActionControl::WebUIPageActionDelegate::GetState() const {
  if (!observation_.IsObserving()) {
    return nullptr;
  }
  const auto* model = observation_.GetSource();
  if (!model->GetVisible()) {
    return nullptr;
  }

  auto state = toolbar_ui_api::mojom::PageActionState::New();
  state->page_action_id = ActionIdToMojomPageActionId(action_id_);
  state->accessible_name = model->GetAccessibleName();
  state->tooltip_text = model->GetTooltipText();
  return state;
}

void WebUIPageActionControl::WebUIPageActionDelegate::NotifyClick(
    PageActionTrigger trigger) {
  click_callback_.Run(trigger);

  action_item_->InvokeAction(
      actions::ActionInvocationContext::Builder()
          .SetProperty(
              page_actions::kPageActionTriggerKey,
              static_cast<
                  std::underlying_type_t<page_actions::PageActionTrigger>>(
                  trigger))
          .SetProperty(
              page_actions::kPageActionEntryPointKey,
              static_cast<
                  std::underlying_type_t<page_actions::PageActionEntryPoint>>(
                  page_actions::PageActionEntryPoint::kSuggestionChip))
          .Build());
}

void WebUIPageActionControl::WebUIPageActionDelegate::
    NotifyChipShowingChanged() {
  if (observation_.IsObserving()) {
    bool is_chip_showing = observation_.GetSource()->ShouldShowSuggestionChip();
    is_chip_showing_changed_callback_.Run(is_chip_showing);
  }
}

WebUIPageActionControl::WebUIPageActionControl(
    actions::ActionItem* root_action_item)
    : root_action_item_(root_action_item) {
  for (actions::ActionId action_id : page_actions::kActionIds) {
    actions::ActionItem* action_item = nullptr;
    if (root_action_item) {
      action_item =
          actions::ActionManager::Get().FindAction(action_id, root_action_item);
    }
    if (action_item) {
      delegates_[action_id] = std::make_unique<WebUIPageActionDelegate>(
          action_id, *action_item, *this);
    }
  }
}

WebUIPageActionControl::~WebUIPageActionControl() = default;

void WebUIPageActionControl::Init(WebUIToolbarControlDelegate* webui_delegate) {
  webui_delegate_ = webui_delegate;
}

void WebUIPageActionControl::UpdateController(
    content::WebContents* web_contents) {
  page_actions::PageActionController* new_controller = nullptr;
  if (web_contents) {
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_contents);
    if (tab) {
      new_controller = page_actions::PageActionController::From(tab);
    }
  }

  if (active_controller_ == new_controller) {
    return;
  }

  active_controller_subscription_ = {};
  active_controller_ = new_controller;

  if (active_controller_) {
    active_controller_subscription_ =
        active_controller_->RegisterOnWillDestroyCallback(
            base::BindOnce(&WebUIPageActionControl::OnControllerDestroying,
                           base::Unretained(this)));
  }

  for (auto& [action_id, delegate] : delegates_) {
    delegate->SetController(active_controller_);
  }
}

std::vector<toolbar_ui_api::mojom::PageActionStatePtr>
WebUIPageActionControl::GetPageActionStates() {
  std::vector<toolbar_ui_api::mojom::PageActionStatePtr> states;
  for (actions::ActionId action_id : page_actions::kActionIds) {
    auto it = delegates_.find(action_id);
    if (it != delegates_.end()) {
      auto state = it->second->GetState();
      if (state) {
        states.push_back(std::move(state));
      }
    }
  }
  return states;
}

void WebUIPageActionControl::OnPageActionClick(
    toolbar_ui_api::mojom::PageActionId action_id,
    PageActionTrigger trigger,
    toolbar_ui_api::mojom::ToolbarUIService::OnPageActionClickCallback
        callback) {
  auto it = delegates_.find(MojomPageActionIdToActionId(action_id));
  if (it == delegates_.end()) {
    std::move(callback).Run(base::unexpected(
        Error::New(Code::kInvalidArgument, "Invalid PageActionId")));
    return;
  }

  it->second->NotifyClick(trigger);
  std::move(callback).Run(std::monostate());
}

void WebUIPageActionControl::OnPageActionChipShowingChanged(
    toolbar_ui_api::mojom::PageActionId action_id,
    toolbar_ui_api::mojom::ToolbarUIService::
        OnPageActionChipShowingChangedCallback callback) {
  auto it = delegates_.find(MojomPageActionIdToActionId(action_id));
  if (it == delegates_.end()) {
    std::move(callback).Run(base::unexpected(
        Error::New(Code::kInvalidArgument, "Invalid PageActionId")));
    return;
  }

  it->second->NotifyChipShowingChanged();
  std::move(callback).Run(std::monostate());
}

void WebUIPageActionControl::NotifyPageActionStateChanged() {
  if (webui_delegate_) {
    webui_delegate_->OnPageActionChanged(GetPageActionStates());
  }
}

void WebUIPageActionControl::OnControllerDestroying(
    page_actions::PageActionController& controller) {
  CHECK_EQ(active_controller_, &controller);
  UpdateController(nullptr);
}

}  // namespace page_actions
