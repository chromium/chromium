// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/webui_page_action_control.h"

#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/page_action/page_action_view_util.h"
#include "chrome/browser/ui/views/page_action/webui_page_action_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/actions/actions.h"

namespace page_actions {

using MojomPageActionId = toolbar_ui_api::mojom::PageActionId;

namespace {

using Code = mojo_base::mojom::Code;
using Error = mojo_base::mojom::Error;

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

  toolbar_ui_api::mojom::PageActionStatePtr GetState();
  void NotifyClick(PageActionTrigger trigger);
  void NotifyChipShowingChanged();

  const page_actions::PageActionModelInterface* GetObservedModel() const {
    return observation_.IsObserving() ? observation_.GetSource() : nullptr;
  }
  page_actions::PageActionController* GetController() const {
    return controller_;
  }

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
  bool was_chip_visible_ = false;

  toolbar_ui_api::IconHandle cached_icon_;
};

void WebUIPageActionControl::WebUIPageActionDelegate::SetController(
    page_actions::PageActionController* controller) {
  observation_.Reset();
  action_item_subscription_ = {};
  controller_ = controller;
  was_chip_visible_ = false;

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
  const bool is_chip_visible =
      model.GetVisible() && model.ShouldShowSuggestionChip();

  if (model.GetShouldAnnounceChip() && !was_chip_visible_ && is_chip_visible) {
    owner_->AnnounceAlert(model.GetText());
  }
  was_chip_visible_ = is_chip_visible;

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
  was_chip_visible_ = false;
  if (old_state_) {
    old_state_ = nullptr;
    owner_->NotifyPageActionStateChanged();
  }
}

toolbar_ui_api::mojom::PageActionStatePtr
WebUIPageActionControl::WebUIPageActionDelegate::GetState() {
  if (!observation_.IsObserving()) {
    return nullptr;
  }
  const auto* model = observation_.GetSource();
  if (!model->GetVisible()) {
    return nullptr;
  }

  auto state = toolbar_ui_api::mojom::PageActionState::New();
  state->page_action_id =
      webui_toolbar::ActionIdToMojomPageActionId(action_id_);
  state->accessible_name = model->GetAccessibleName();
  state->tooltip_text = model->GetTooltipText();
  state->icon = cached_icon_ =
      owner_->webui_delegate_->GetIconTable().RegisterImageModelTryReuse(
          model->GetImage(), cached_icon_);
  return state;
}

void WebUIPageActionControl::WebUIPageActionDelegate::NotifyClick(
    PageActionTrigger trigger) {
  click_callback_.Run(trigger);

  auto builder =
      actions::ActionInvocationContext::Builder()
          .SetProperty(page_actions::kPageActionTriggerKey, trigger)
          .SetProperty(page_actions::kPageActionEntryPointKey,
                       page_actions::PageActionEntryPoint::kSuggestionChip);
  if (auto side_panel_trigger =
          GetSidePanelOpenTriggerForPageAction(action_item_->GetActionId())) {
    builder = std::move(builder).SetProperty(kSidePanelOpenTriggerKey,
                                             *side_panel_trigger);
  }
  action_item_->InvokeAction(std::move(builder).Build());
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
      views_[action_id] =
          std::make_unique<WebUIPageActionView>(action_id, *this);
    }
  }
}

WebUIPageActionControl::~WebUIPageActionControl() = default;

void WebUIPageActionControl::Init(WebUIToolbarControlDelegate* webui_delegate) {
  webui_delegate_ = webui_delegate;
}

PageActionViewInterface* WebUIPageActionControl::GetPageActionViewInterface(
    actions::ActionId action_id) {
  auto it = views_.find(action_id);
  if (it != views_.end()) {
    return it->second.get();
  }
  return nullptr;
}

BrowserWindowInterface* WebUIPageActionControl::GetBrowser() {
  return webui_delegate_ ? webui_delegate_->GetBrowser() : nullptr;
}

const page_actions::PageActionModelInterface*
WebUIPageActionControl::GetObservedModel(actions::ActionId action_id) const {
  auto it = delegates_.find(action_id);
  if (it != delegates_.end()) {
    return it->second->GetObservedModel();
  }
  return nullptr;
}

page_actions::PageActionController* WebUIPageActionControl::GetController(
    actions::ActionId action_id) {
  auto it = delegates_.find(action_id);
  if (it != delegates_.end()) {
    return it->second->GetController();
  }
  return nullptr;
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

void WebUIPageActionControl::SetShouldHidePageActions(
    bool should_hide_page_actions) {
  if (active_controller_) {
    active_controller_->SetShouldHidePageActions(should_hide_page_actions);
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
  auto it =
      delegates_.find(webui_toolbar::MojomPageActionIdToActionId(action_id));
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
  auto it =
      delegates_.find(webui_toolbar::MojomPageActionIdToActionId(action_id));
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

void WebUIPageActionControl::AnnounceAlert(const std::u16string& announcement) {
  if (webui_delegate_) {
    webui_delegate_->AnnounceAlert(announcement);
  }
}

void WebUIPageActionControl::OnControllerDestroying(
    page_actions::PageActionController& controller) {
  CHECK_EQ(active_controller_, &controller);
  UpdateController(nullptr);
}

}  // namespace page_actions
