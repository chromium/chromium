// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/profile_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"

ToolbarActionsModel::ToolbarActionsModel(
    Profile* profile,
    extensions::ExtensionPrefs* extension_prefs)
    : profile_(profile),
      extension_prefs_(extension_prefs),
      prefs_(profile_->GetPrefs()),
      extension_action_api_(extensions::ExtensionActionAPI::Get(profile_)),
      extension_registry_(extensions::ExtensionRegistry::Get(profile_)),
      extension_action_manager_(
          extensions::ExtensionActionManager::Get(profile_)),
      actions_initialized_(false) {
  extensions::ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE, base::BindOnce(&ToolbarActionsModel::OnReady,
                                weak_ptr_factory_.GetWeakPtr()));

  // We only care about watching toolbar-order prefs if not in incognito mode.
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(
      extensions::pref_names::kPinnedExtensions,
      base::BindRepeating(&ToolbarActionsModel::UpdatePinnedActionIds,
                          base::Unretained(this)));
}

ToolbarActionsModel::~ToolbarActionsModel() {}

// static
ToolbarActionsModel* ToolbarActionsModel::Get(Profile* profile) {
  return ToolbarActionsModelFactory::GetForProfile(profile);
}

void ToolbarActionsModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ToolbarActionsModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ToolbarActionsModel::OnExtensionActionUpdated(
    extensions::ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  NotifyToolbarActionUpdated(extension_action->extension_id());
}

void ToolbarActionsModel::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  // We don't want to add the same extension twice. It may have already been
  // added by EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED below, if the user
  // hides the browser action and then disables and enables the extension.
  if (!HasAction(extension->id()) && ShouldAddExtension(extension))
    AddAction(extension->id());
}

void ToolbarActionsModel::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  RemoveAction(extension->id());
}

void ToolbarActionsModel::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (profile_->IsOffTheRecord()) {
    // The on-the-record version will update the prefs; incognito is read-only.
    return;
  }

  // Remove the extension id from the ordered list, if it exists (the extension
  // might not be represented in the list because it might not have an icon).
  RemovePref(extension->id());
}

void ToolbarActionsModel::OnExtensionManagementSettingsChanged() {
  UpdatePinnedActionIds();
}

void ToolbarActionsModel::OnExtensionPermissionsUpdated(
    const extensions::Extension& extension,
    const extensions::PermissionSet& permissions,
    extensions::PermissionsManager::UpdateReason reason) {
  NotifyToolbarActionUpdated(extension.id());
}

void ToolbarActionsModel::OnActiveTabPermissionGranted(
    const extensions::Extension& extension) {
  NotifyToolbarActionUpdated(extension.id());
}

void ToolbarActionsModel::Shutdown() {
  permissions_manager_observation_.Reset();
}

void ToolbarActionsModel::RemovePref(const ActionId& action_id) {
  // The extension is already unloaded at this point, and so shouldn't be in
  // the active pinned set.
  DCHECK(!IsActionPinned(action_id));
  auto stored_pinned_actions = extension_prefs_->GetPinnedExtensions();
  auto iter = base::ranges::find(stored_pinned_actions, action_id);
  if (iter != stored_pinned_actions.end()) {
    stored_pinned_actions.erase(iter);
    extension_prefs_->SetPinnedExtensions(stored_pinned_actions);
  }
}

void ToolbarActionsModel::OnReady() {
  InitializeActionList();

  // Wait until the extension system is ready before observing any further
  // changes so that the toolbar buttons can be shown in their stable ordering
  // taken from prefs.
  extension_registry_observation_.Observe(extension_registry_.get());
  extension_action_observation_.Observe(extension_action_api_.get());
  permissions_manager_observation_.Observe(
      extensions::PermissionsManager::Get(profile_));

  auto* management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  extension_management_observation_.Observe(management);

  actions_initialized_ = true;
  for (Observer& observer : observers_)
    observer.OnToolbarModelInitialized();
}

bool ToolbarActionsModel::ShouldAddExtension(
    const extensions::Extension* extension) {
  // In incognito mode, don't add any extensions that aren't incognito-enabled.
  if (profile_->IsOffTheRecord() &&
      !extensions::util::IsIncognitoEnabled(extension->id(), profile_))
    return false;

  // In this case, we don't care about the browser action visibility, because
  // we want to show each extension regardless.
  return extension_action_manager_->GetExtensionAction(*extension) != nullptr;
}

void ToolbarActionsModel::AddAction(const ActionId& action_id) {
  // We only use AddAction() once the system is initialized.
  CHECK(actions_initialized_);

  action_ids_.insert(action_id);

  for (Observer& observer : observers_)
    observer.OnToolbarActionAdded(action_id);

  UpdatePinnedActionIds();
}

void ToolbarActionsModel::RemoveAction(const ActionId& action_id) {
  const bool did_erase = action_ids_.erase(action_id) > 0;
  // TODO(devlin): Can we DCHECK did_erase?
  if (!did_erase)
    return;

  UpdatePinnedActionIds();

  for (Observer& observer : observers_)
    observer.OnToolbarActionRemoved(action_id);
}

const std::u16string ToolbarActionsModel::GetExtensionName(
    const ActionId& action_id) const {
  return base::UTF8ToUTF16(
      extension_registry_->enabled_extensions().GetByID(action_id)->name());
}

bool ToolbarActionsModel::HasAction(const ActionId& action_id) const {
  return base::Contains(action_ids_, action_id);
}

bool ToolbarActionsModel::CanShowActionsInToolbar(const Browser& browser) {
  // Pinning extensions is not available in PWAs.
  return !web_app::AppBrowserController::IsWebApp(&browser);
}

bool ToolbarActionsModel::IsRestrictedUrl(const GURL& url) const {
  // We consider a site to be restricted if it's restricted for every
  // extension in the toolbar. This can vary based on the extensions
  // installed - if the user has an extension that can execute script
  // everywhere and has an icon in the toolbar (like the non-ChromeOS version
  // of ChromeVox), then otherwise-restricted sites may not be.
  // If nay extension has access, we want to properly message that (since
  // saying "No extensions can run..." is inaccurate). Other extensions
  // will still be properly attributed in UI.
  return base::ranges::all_of(action_ids(), [this, url](ActionId id) {
    // action_ids() could include disabled extensions that haven't been removed
    // yet from the set due to race conditions. Thus, we don't consider them in
    // the restricted url computation.
    auto* extension = GetExtensionById(id);
    if (!extension) {
      return true;
    }

    return extension->permissions_data()->IsRestrictedUrl(url,
                                                          /*error=*/nullptr);
  });
}

bool ToolbarActionsModel::IsPolicyBlockedHost(const GURL& url) const {
  extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile_)->management_policy();
  auto is_enterprise_extension =
      [policy](const extensions::Extension& extension) {
        return !policy->UserMayModifySettings(&extension, nullptr) ||
               policy->MustRemainInstalled(&extension, nullptr);
      };

  // `url` is NOT a policy-blockedsite when there are no extensions installed.
  if (action_ids().empty()) {
    return false;
  }

  for (auto& action_id : action_ids()) {
    // Skip enterprise extensions since they could still access policy-blocked
    // sites.
    const extensions::Extension* extension = GetExtensionById(action_id);
    if (is_enterprise_extension(*extension)) {
      continue;
    }

    // `url` is NOT a policy-blocked sit when it's allowed for any
    // non-enterprise extension.
    if (!extension->permissions_data()->IsPolicyBlockedHost(url)) {
      return false;
    }
  }

  // `url` is a policy-blocked site when it's blocked for every non-enterprise
  // extension.
  return true;
}

bool ToolbarActionsModel::IsActionPinned(const ActionId& action_id) const {
  return base::Contains(pinned_action_ids_, action_id);
}

bool ToolbarActionsModel::IsActionForcePinned(const ActionId& action_id) const {
  auto* management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  return base::Contains(management->GetForcePinnedList(), action_id);
}

void ToolbarActionsModel::MovePinnedAction(const ActionId& action_id,
                                           size_t target_index) {
  // TODO(crbug.com/40204281): This code assumes all actions are in
  // stored_pinned_actions, which force-pinned actions aren't; so, always keep
  // them 'to the right' of other actions. Remove this guard if we ever add
  // force-pinned actions to the pref.
  if (IsActionForcePinned(action_id))
    return;

  // If pinned actions are empty, we're going to have a real bad time (with
  // out Keep this a hard CHECK (not a DCHECK).
  CHECK(!pinned_action_ids_.empty());
  DCHECK(!profile_->IsOffTheRecord())
      << "Changing action position is disallowed in incognito.";

  auto current_position_on_toolbar =
      base::ranges::find(pinned_action_ids_, action_id);
  CHECK(current_position_on_toolbar != pinned_action_ids_.end(),
        base::NotFatalUntil::M130);
  size_t current_index_on_toolbar =
      current_position_on_toolbar - pinned_action_ids_.begin();

  if (current_index_on_toolbar == target_index)
    return;

  bool is_left_to_right_move = target_index > current_index_on_toolbar;

  // Moving pinned actions is a bit tricky (unless we move it to the end - in
  // which case it's trivial). We need to store the updated state in prefs, but
  // the prefs also contain pin state information for unloaded (but still
  // installed) extensions. Thus, we can't just reorder the pinned_action_ids_
  // (which only include loaded extensions), and set those directly.
  //
  // Instead, we look at the destination of the action in the toolbar, and
  // find the ID of the action to its right (if any). Then in the stored prefs,
  // find that action, and insert the moved action to its left.
  //
  // To further complicate things, force-pinned actions are stored in
  // |pinned_action_ids_| but not in the pref (crbug.com/1266952). So we have to
  // find the ID not just of the action to its right, but the first action to
  // its right that is *not* force-pinned.
  //
  // For example:
  // Consider the pinned extension order in prefs is "A [B C] D E", where
  // B and C are unloaded extensions. Assume we want to A to index 1 on the
  // toolbar (swapping A and D). We would look for the new action to its
  // right (E), and insert it in prefs to the left of it. Thus, the new pref
  // order would be "[B C] D A E".

  // Force-pinned neighbors aren't saved in the pref, so find the preceding,
  // non-force-pinned neighbor. This basically keeps force-pinned actions on the
  // right at all times.
  //
  // TODO(crbug.com/40204281): Simplify this logic when force-pinned extensions
  // are saved in the pref.
  std::vector<ActionId>::iterator non_force_pinned_neighbor =
      pinned_action_ids_.end();
  if (is_left_to_right_move) {
    // LTR move. Starting with the extension to the right of the desired
    // location, do an RTL search for the first non-force-pinned extension.
    // Note: there's always an extension that matches these criteria (this
    // one!).

    // Avoid array bounds shenanigans when target_index >= n.
    auto search_start = std::max(pinned_action_ids_.rend() - target_index - 1,
                                 pinned_action_ids_.rbegin());
    auto reverse_iter = std::find_if(
        search_start, pinned_action_ids_.rend(),
        [this](const ActionId& id) { return !this->IsActionForcePinned(id); });
    non_force_pinned_neighbor = reverse_iter.base();
  } else {
    // RTL move. Starting with the extension to the left of the desired
    // location, do an LTR search for the first non-force-pinned extension.
    // Note: there's always an extension that matches these criteria (this
    // one!).
    non_force_pinned_neighbor = std::find_if(
        pinned_action_ids_.begin() + target_index, pinned_action_ids_.end(),
        [this](const ActionId& id) { return !this->IsActionForcePinned(id); });
  }

  auto stored_pinned_actions = extension_prefs_->GetPinnedExtensions();
  const bool move_to_end =
      non_force_pinned_neighbor == pinned_action_ids_.end();
  auto target_position = move_to_end
                             ? stored_pinned_actions.end()
                             : base::ranges::find(stored_pinned_actions,
                                                  *non_force_pinned_neighbor);

  auto current_position_in_prefs =
      base::ranges::find(stored_pinned_actions, action_id);
  CHECK(current_position_in_prefs != stored_pinned_actions.end(),
        base::NotFatalUntil::M130);

  // Rotate |action_id| to be in the target position.
  if (is_left_to_right_move) {
    std::rotate(current_position_in_prefs, std::next(current_position_in_prefs),
                target_position);
  } else {
    std::rotate(target_position, current_position_in_prefs,
                std::next(current_position_in_prefs));
  }

  extension_prefs_->SetPinnedExtensions(stored_pinned_actions);
  // The |pinned_action_ids_| should be updated as a result of updating the
  // preference.
  DCHECK(pinned_action_ids_ == GetFilteredPinnedActionIds());
}

// Combine the currently enabled extensions that have browser actions (which
// we get from the ExtensionRegistry) with the ordering we get from the pref
// service. For robustness we use a somewhat inefficient process:
// 1. Create a vector of actions sorted by their pref values. This vector may
// have holes.
// 2. Create a vector of actions that did not have a pref value.
// 3. Remove holes from the sorted vector and append the unsorted vector.
void ToolbarActionsModel::InitializeActionList() {
  CHECK(action_ids_.empty());  // We shouldn't have any actions yet.

  if (profile_->IsOffTheRecord())
    IncognitoPopulate();
  else
    Populate();

  // Set |pinned_action_ids_| directly to avoid notifying observers that they
  // have changed even though they haven't.
  pinned_action_ids_ = GetFilteredPinnedActionIds();

  if (!profile_->IsOffTheRecord()) {
    // Prefixed with "ExtensionToolbarModel" rather than
    // "Extensions.Toolbar" for historical reasons.
    base::UmaHistogramCounts100("ExtensionToolbarModel.BrowserActionsCount",
                                action_ids_.size());
    if (extensions::profile_util::ProfileCanUseNonComponentExtensions(
            profile_)) {
      base::UmaHistogramCounts100("Extension.Toolbar.BrowserActionsCount2",
                                  action_ids_.size());
    }
    if (!action_ids_.empty()) {
      base::UmaHistogramCounts100("Extensions.Toolbar.PinnedExtensionCount2",
                                  pinned_action_ids_.size());
      double percentage_double =
          static_cast<double>(pinned_action_ids_.size()) / action_ids_.size() *
          100.0;
      base::UmaHistogramPercentageObsoleteDoNotUse(
          "Extensions.Toolbar.PinnedExtensionPercentage3",
          base::ClampRound(percentage_double));
    }
  }
}

void ToolbarActionsModel::Populate() {
  DCHECK(!profile_->IsOffTheRecord());

  // Add the extension action ids to all_actions.
  const extensions::ExtensionSet& extensions =
      extension_registry_->enabled_extensions();
  for (const scoped_refptr<const extensions::Extension>& extension :
       extensions) {
    if (!ShouldAddExtension(extension.get()))
      continue;
    action_ids_.insert(extension->id());
  }
}

void ToolbarActionsModel::IncognitoPopulate() {
  DCHECK(profile_->IsOffTheRecord());
  const ToolbarActionsModel* original_model =
      ToolbarActionsModel::Get(profile_->GetOriginalProfile());

  // Only extensions enabled in incognito mode are added to the incognito mode
  // toolbar.
  base::flat_set<ActionId> incognito_ids = original_model->action_ids_;
  base::EraseIf(incognito_ids, [this](const ActionId& id) {
    return !ShouldAddExtension(GetExtensionById(id));
  });
  action_ids_ = std::move(incognito_ids);
}

void ToolbarActionsModel::SetActionVisibility(const ActionId& action_id,
                                              bool is_now_visible) {
  DCHECK_NE(is_now_visible, IsActionPinned(action_id));
  DCHECK(!IsActionForcePinned(action_id));
  DCHECK(!profile_->IsOffTheRecord())
      << "Changing action pin state is disallowed in incognito.";

  auto stored_pinned_action_ids = extension_prefs_->GetPinnedExtensions();
  DCHECK_NE(is_now_visible,
            base::Contains(stored_pinned_action_ids, action_id));
  if (is_now_visible) {
    stored_pinned_action_ids.push_back(action_id);
  } else {
    std::erase(stored_pinned_action_ids, action_id);
  }
  extension_prefs_->SetPinnedExtensions(stored_pinned_action_ids);
  // The |pinned_action_ids_| should be updated as a result of updating the
  // preference.
  DCHECK(pinned_action_ids_ == GetFilteredPinnedActionIds());

  extension_action_api_->OnActionPinnedStateChanged(action_id, is_now_visible);
}

const extensions::Extension* ToolbarActionsModel::GetExtensionById(
    const ActionId& action_id) const {
  return extension_registry_->enabled_extensions().GetByID(action_id);
}

void ToolbarActionsModel::UpdatePinnedActionIds() {
  // If extensions are not ready, defer to later Populate() call.
  if (!actions_initialized_)
    return;

  std::vector<ActionId> pinned_extensions = GetFilteredPinnedActionIds();
  if (pinned_extensions == pinned_action_ids_)
    return;

  pinned_action_ids_ = pinned_extensions;
  for (Observer& observer : observers_)
    observer.OnToolbarPinnedActionsChanged();
}

std::vector<ToolbarActionsModel::ActionId>
ToolbarActionsModel::GetFilteredPinnedActionIds() const {
  // Force-pinned extensions should always be present in the output vector.
  extensions::ExtensionIdList pinned = extension_prefs_->GetPinnedExtensions();
  auto* management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  // O(n^2), but there are typically very few force-pinned extensions.
  base::ranges::copy_if(
      management->GetForcePinnedList(), std::back_inserter(pinned),
      [&pinned](const std::string& id) { return !base::Contains(pinned, id); });

  // TODO(pbos): Make sure that the pinned IDs are pruned from ExtensionPrefs on
  // startup so that we don't keep saving stale IDs.
  std::vector<ActionId> filtered_action_ids;
  for (auto& action_id : pinned) {
    if (HasAction(action_id))
      filtered_action_ids.push_back(action_id);
  }
  return filtered_action_ids;
}

void ToolbarActionsModel::NotifyToolbarActionUpdated(
    const ActionId& action_id) {
  if (!HasAction(action_id)) {
    return;
  }

  for (Observer& observer : observers_) {
    observer.OnToolbarActionUpdated(action_id);
  }
}
