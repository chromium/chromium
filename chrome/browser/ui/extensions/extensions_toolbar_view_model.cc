// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
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
  auto* tab_list = TabListInterface::From(browser_);
  if (tab_list) {
    tab_list_observation_.Observe(tab_list);
  }

  permissions_manager_observation_.Observe(
      extensions::PermissionsManager::Get(browser_->GetProfile()));

  if (actions_model_->actions_initialized()) {
    OnToolbarModelInitialized();
  }
}

ExtensionsToolbarViewModel::~ExtensionsToolbarViewModel() {
  WebContentsObserver::Observe(nullptr);
}

ExtensionsToolbarViewModel::RequestAccessButtonParams::
    RequestAccessButtonParams() = default;
ExtensionsToolbarViewModel::RequestAccessButtonParams::
    RequestAccessButtonParams(RequestAccessButtonParams&&) = default;
ExtensionsToolbarViewModel::RequestAccessButtonParams&
ExtensionsToolbarViewModel::RequestAccessButtonParams::operator=(
    RequestAccessButtonParams&&) = default;
ExtensionsToolbarViewModel::RequestAccessButtonParams::
    ~RequestAccessButtonParams() = default;

void ExtensionsToolbarViewModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionsToolbarViewModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ToolbarActionViewModel* ExtensionsToolbarViewModel::GetActionModelForId(
    const ToolbarActionsModel::ActionId& action_id) const {
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

ExtensionsToolbarViewModel::ExtensionsToolbarButtonState
ExtensionsToolbarViewModel::GetButtonState(
    content::WebContents& web_contents) const {
  Profile* profile = browser_->GetProfile();
  const GURL& url = web_contents.GetLastCommittedURL();

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

// Extensions are included in the request access button only when:
//   - site allows customizing site access by extension
//   - extension added a request that has not been dismissed
//   - requests can be shown in the toolbar
ExtensionsToolbarViewModel::RequestAccessButtonParams
ExtensionsToolbarViewModel::GetRequestAccessButtonParams(
    content::WebContents* web_contents) const {
  RequestAccessButtonParams params;
  if (!web_contents) {
    return params;
  }

  Profile* profile = browser_->GetProfile();
  extensions::PermissionsManager* permissions_manager =
      extensions::PermissionsManager::Get(profile);
  auto origin = web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  extensions::PermissionsManager::UserSiteSetting site_setting =
      permissions_manager->GetUserSiteSetting(origin);

  if (site_setting !=
      extensions::PermissionsManager::UserSiteSetting::kCustomizeByExtension) {
    return params;
  }

  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  extensions::SitePermissionsHelper site_permissions_helper(profile);

  std::vector<std::u16string> extension_names;
  for (const auto& action_id : actions_model_->action_ids()) {
    bool has_active_request =
        permissions_manager->HasActiveHostAccessRequest(tab_id, action_id);
    bool can_show_access_requests_in_toolbar =
        site_permissions_helper.ShowAccessRequestsInToolbar(action_id);

    if (has_active_request && can_show_access_requests_in_toolbar) {
      params.extension_ids.push_back(action_id);
      ToolbarActionViewModel* action_model = GetActionModelForId(action_id);
      // If an extension has an active request, it should have an action model.
      CHECK(action_model);
      extension_names.push_back(action_model->GetActionName());
    }
  }

  if (params.extension_ids.empty()) {
    return params;
  }

  std::vector<std::u16string> tooltip_parts;
  tooltip_parts.push_back(l10n_util::GetStringFUTF16(
      IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_TOOLTIP_MULTIPLE_EXTENSIONS,
      extensions::ui_util::GetFormattedHostForDisplay(*web_contents)));
  tooltip_parts.insert(tooltip_parts.end(), extension_names.begin(),
                       extension_names.end());
  params.tooltip_text = base::JoinString(tooltip_parts, u"\n");

  return params;
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
  if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted()) {
    return;
  }
  for (Observer& obs : observers_) {
    obs.OnActiveWebContentsChanged(handle->IsSameDocument());
  }
}

void ExtensionsToolbarViewModel::OnActiveTabChanged(TabListInterface& tab_list,
                                                    tabs::TabInterface* tab) {
  content::WebContents* contents = tab->GetContents();
  WebContentsObserver::Observe(contents);
  for (Observer& obs : observers_) {
    obs.OnActiveWebContentsChanged(/*is_same_document=*/false);
  }
}

void ExtensionsToolbarViewModel::OnTabListDestroyed(
    TabListInterface& tab_list) {
  tab_list_observation_.Reset();
}

bool ExtensionsToolbarViewModel::AnyActionHasCurrentSiteAccess(
    content::WebContents& web_contents) const {
  for (const auto& [action_id, model] : actions_) {
    if (model->GetSiteInteraction(&web_contents) ==
        extensions::SitePermissionsHelper::SiteInteraction::kGranted) {
      return true;
    }
  }
  return false;
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

void ExtensionsToolbarViewModel::OnHostAccessRequestAdded(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  content::WebContents* web_contents = GetCurrentWebContents();
  int current_tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id != current_tab_id) {
    return;
  }
  for (Observer& obs : observers_) {
    obs.OnRequestAccessButtonParamsChanged(web_contents);
  }
}

void ExtensionsToolbarViewModel::OnHostAccessRequestUpdated(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  content::WebContents* web_contents = GetCurrentWebContents();
  int current_tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id != current_tab_id) {
    return;
  }
  for (Observer& obs : observers_) {
    obs.OnRequestAccessButtonParamsChanged(web_contents);
  }
}

void ExtensionsToolbarViewModel::OnHostAccessRequestRemoved(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  content::WebContents* web_contents = GetCurrentWebContents();
  int current_tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id != current_tab_id) {
    return;
  }
  for (Observer& obs : observers_) {
    obs.OnRequestAccessButtonParamsChanged(web_contents);
  }
}

void ExtensionsToolbarViewModel::OnHostAccessRequestsCleared(int tab_id) {
  content::WebContents* web_contents = GetCurrentWebContents();
  int current_tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id != current_tab_id) {
    return;
  }
  for (Observer& obs : observers_) {
    obs.OnRequestAccessButtonParamsChanged(web_contents);
  }
}

void ExtensionsToolbarViewModel::OnHostAccessRequestDismissedByUser(
    const extensions::ExtensionId& extension_id,
    const url::Origin& origin) {
  content::WebContents* web_contents = GetCurrentWebContents();
  for (Observer& obs : observers_) {
    obs.OnRequestAccessButtonParamsChanged(web_contents);
  }
}

void ExtensionsToolbarViewModel::OnUserPermissionsSettingsChanged(
    const extensions::PermissionsManager::UserPermissionsSettings& settings) {
  for (Observer& obs : observers_) {
    obs.OnToolbarControlStateUpdated();
  }
  // TODO(crbug.com/40857356): Update request access button hover card. This
  // will be slightly different than 'OnToolbarActionUpdated' since site
  // settings update are not tied to a specific action.
}

void ExtensionsToolbarViewModel::OnShowAccessRequestsInToolbarChanged(
    const extensions::ExtensionId& extension_id,
    bool can_show_requests) {
  content::WebContents* web_contents = GetCurrentWebContents();
  for (Observer& obs : observers_) {
    obs.OnRequestAccessButtonParamsChanged(web_contents);
  }
}
