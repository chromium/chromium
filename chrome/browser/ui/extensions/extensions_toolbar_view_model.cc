// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "url/origin.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::WebContentsObserver;

ExtensionsToolbarViewModel::ExtensionsToolbarViewModel(
    Delegate* delegate,
    BrowserWindowInterface* browser,
    ToolbarActionsModel* actions_model)
    : browser_(browser), delegate_(delegate), actions_model_(actions_model) {
  WebContentsObserver::Observe(GetCurrentWebContents());
  actions_model_observation_.Observe(actions_model_);
  tab_list_observation_.Observe(TabListInterface::From(browser_));

  if (actions_model_->actions_initialized()) {
    OnToolbarModelInitialized();
  }
}

ExtensionsToolbarViewModel::~ExtensionsToolbarViewModel() {
  WebContentsObserver::Observe(nullptr);
}

void ExtensionsToolbarViewModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionsToolbarViewModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ToolbarActionViewModel* ExtensionsToolbarViewModel::GetActionModelForId(
    const ToolbarActionsModel::ActionId& action_id) {
  auto it = actions_.find(action_id);
  if (it == actions_.end()) {
    return nullptr;
  }
  return it->second.get();
}

void ExtensionsToolbarViewModel::MovePinnedAction(
    const ToolbarActionsModel::ActionId& action_id,
    size_t target_index) {
  actions_model_->MovePinnedAction(action_id, target_index);
}

void ExtensionsToolbarViewModel::MovePinnedActionBy(
    const std::string& action_id,
    int move_by) {
  // Find the action's current index and verify that it's currently pinned.
  auto iter = std::ranges::find(actions_model_->pinned_action_ids(), action_id);
  CHECK(iter != actions_model_->pinned_action_ids().cend());

  // Calculate the target index, clamping it between 0 and `size - 1` to prevent
  // out-of-bounds errors.
  int current_index = iter - actions_model_->pinned_action_ids().cbegin();
  int new_index = std::clamp(
      current_index + move_by, 0,
      static_cast<int>(actions_model_->pinned_action_ids().size()) - 1);
  if (new_index == current_index) {
    return;
  }
  MovePinnedAction(action_id, new_index);
}

const base::flat_set<ToolbarActionsModel::ActionId>&
ExtensionsToolbarViewModel::GetAllActionIds() const {
  return actions_model_->action_ids();
}

const std::vector<ToolbarActionsModel::ActionId>&
ExtensionsToolbarViewModel::GetPinnedActionIds() const {
  return actions_model_->pinned_action_ids();
}

bool ExtensionsToolbarViewModel::AreActionsInitialized() {
  return actions_model_->actions_initialized();
}

bool ExtensionsToolbarViewModel::AnyActionHasCurrentSiteAccess(
    content::WebContents* web_contents) const {
  for (const auto& [action_id, model] : actions_) {
    if (model->GetSiteInteraction(web_contents) ==
        extensions::SitePermissionsHelper::SiteInteraction::kGranted) {
      return true;
    }
  }
  return false;
}

ExtensionsToolbarViewModel::ExtensionsToolbarButtonState
ExtensionsToolbarViewModel::GetButtonState(
    content::WebContents* web_contents) const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  const GURL& url = web_contents->GetLastCommittedURL();

  if (actions_model_->IsRestrictedUrl(url)) {
    return ExtensionsToolbarButtonState::kAllExtensionsBlocked;
  }

  extensions::PermissionsManager* manager =
      extensions::PermissionsManager::Get(profile);
  extensions::PermissionsManager::UserSiteSetting site_setting =
      manager->GetUserSiteSetting(url::Origin::Create(url));

  if (site_setting ==
      extensions::PermissionsManager::UserSiteSetting::kBlockAllExtensions) {
    return ExtensionsToolbarButtonState::kAllExtensionsBlocked;
  }

  if (AnyActionHasCurrentSiteAccess(web_contents)) {
    return ExtensionsToolbarButtonState::kAnyExtensionHasAccess;
  }

  return ExtensionsToolbarButtonState::kDefault;
}

void ExtensionsToolbarViewModel::ExecuteUserAction(
    const ToolbarActionsModel::ActionId& action_id,
    ToolbarActionViewModel::InvocationSource source) {
  GetActionModelForId(action_id)->ExecuteUserAction(source);
}

ToolbarActionViewModel* ExtensionsToolbarViewModel::GetActionForId(
    const std::string& action_id) {
  return GetActionModelForId(action_id);
}

void ExtensionsToolbarViewModel::HideActivePopup() {
  delegate_->HideActivePopup();
}

bool ExtensionsToolbarViewModel::CloseOverflowMenuIfOpen() {
  return delegate_->CloseOverflowMenuIfOpen();
}

bool ExtensionsToolbarViewModel::ShowToolbarActionPopupForAPICall(
    const std::string& action_id,
    ShowPopupCallback callback) {
  if (!delegate_->CanShowToolbarActionPopupForAPICall(action_id)) {
    return false;
  }

  ToolbarActionViewModel* action = GetActionModelForId(action_id);
  DCHECK(action);
  action->TriggerPopupForAPI(std::move(callback));
  return true;
}

void ExtensionsToolbarViewModel::ToggleExtensionsMenu() {
  delegate_->ToggleExtensionsMenu();
}

bool ExtensionsToolbarViewModel::HasAnyExtensions() const {
  return !GetAllActionIds().empty();
}

void ExtensionsToolbarViewModel::OnToolbarModelInitialized() {
  CHECK(actions_.empty());
  CHECK(actions_model_->actions_initialized());

  // Create a vector first to initialize flat_map more efficiently.
  std::vector<std::pair<ToolbarActionsModel::ActionId,
                        std::unique_ptr<ToolbarActionViewModel>>>
      initial_actions;
  initial_actions.reserve(actions_model_->action_ids().size());
  for (const auto& action_id : actions_model_->action_ids()) {
    initial_actions.emplace_back(
        action_id, delegate_->CreateActionViewModel(action_id, this));
  }
  actions_ = base::flat_map<ToolbarActionsModel::ActionId,
                            std::unique_ptr<ToolbarActionViewModel>>(
      std::move(initial_actions));

  for (Observer& obs : observers_) {
    obs.OnActionsInitialized();
  }
}

void ExtensionsToolbarViewModel::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  AppendActionModel(action_id);

  for (Observer& obs : observers_) {
    obs.OnActionAdded(action_id);
  }
}

void ExtensionsToolbarViewModel::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  auto iter = actions_.find(action_id);
  CHECK(iter != actions_.end());

  // Transfer ownership to a local variable to ensure the model remains alive
  // during the subsequent UI cleanup notifications.
  std::unique_ptr<ToolbarActionViewModel> model = std::move(iter->second);

  actions_.erase(iter);

  for (Observer& obs : observers_) {
    obs.OnActionRemoved(action_id);
  }
}

void ExtensionsToolbarViewModel::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  for (Observer& obs : observers_) {
    obs.OnActionUpdated(action_id);
  }
}

void ExtensionsToolbarViewModel::OnToolbarPinnedActionsChanged() {
  for (Observer& obs : observers_) {
    obs.OnPinnedActionsChanged();
  }
}

void ExtensionsToolbarViewModel::DidFinishNavigation(
    content::NavigationHandle* handle) {
  for (Observer& obs : observers_) {
    obs.OnActiveWebContentsChanged();
  }
}

void ExtensionsToolbarViewModel::OnActiveTabChanged(tabs::TabInterface* tab) {
  WebContentsObserver::Observe(tab->GetContents());
  for (Observer& obs : observers_) {
    obs.OnActiveWebContentsChanged();
  }
}

void ExtensionsToolbarViewModel::OnTabListDestroyed(
    TabListInterface& tab_list) {
  tab_list_observation_.Reset();
}

void ExtensionsToolbarViewModel::AppendActionModel(
    const ToolbarActionsModel::ActionId& action_id) {
  actions_.emplace(action_id,
                   delegate_->CreateActionViewModel(action_id, this));
}

content::WebContents* ExtensionsToolbarViewModel::GetCurrentWebContents()
    const {
  tabs::TabInterface* tab = TabListInterface::From(browser_)->GetActiveTab();
  if (!tab) {
    return nullptr;
  }
  return tab->GetContents();
}
