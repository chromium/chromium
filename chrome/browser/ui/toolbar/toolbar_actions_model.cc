// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/one_shot_event.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_constants.h"

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
      actions_initialized_(false),
      highlight_type_(HIGHLIGHT_NONE),
      has_active_bubble_(false) {
  extensions::ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE, base::BindOnce(&ToolbarActionsModel::OnReady,
                                weak_ptr_factory_.GetWeakPtr()));
  visible_icon_count_ =
      prefs_->GetInteger(extensions::pref_names::kToolbarSize);

  // We only care about watching toolbar-order prefs if not in incognito mode.
  const bool watch_toolbar_order = !profile_->IsOffTheRecord();
  const bool watch_pinned_extensions =
      base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu);
  if (watch_toolbar_order || watch_pinned_extensions) {
    pref_change_registrar_.Init(prefs_);
    pref_change_callback_ =
        base::Bind(&ToolbarActionsModel::OnActionToolbarPrefChange,
                   base::Unretained(this));

    if (watch_toolbar_order) {
      pref_change_registrar_.Add(extensions::pref_names::kToolbar,
                                 pref_change_callback_);
    }

    if (watch_pinned_extensions) {
      pref_change_registrar_.Add(extensions::pref_names::kPinnedExtensions,
                                 pref_change_callback_);
    }
  }
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

void ToolbarActionsModel::MoveActionIcon(const ActionId& id, size_t index) {
  auto pos = action_ids_.begin();
  while (pos != action_ids_.end() && *pos != id)
    ++pos;
  if (pos == action_ids_.end()) {
    NOTREACHED();
    return;
  }

  ActionId action = *pos;
  action_ids_.erase(pos);

  auto pos_id =
      std::find(last_known_positions_.begin(), last_known_positions_.end(), id);
  if (pos_id != last_known_positions_.end())
    last_known_positions_.erase(pos_id);

  if (index < action_ids_.size()) {
    // If the index is not at the end, find the action currently at |index|, and
    // insert |action| before it in |action_ids_| and |action|'s id in
    // |last_known_positions_|.
    auto iter = action_ids_.begin() + index;
    last_known_positions_.insert(std::find(last_known_positions_.begin(),
                                           last_known_positions_.end(), *iter),
                                 id);
    action_ids_.insert(iter, action);
  } else {
    // Otherwise, put |action| and |id| at the end.
    DCHECK_EQ(action_ids_.size(), index);
    action_ids_.push_back(action);
    last_known_positions_.push_back(id);
  }

  for (Observer& observer : observers_)
    observer.OnToolbarActionMoved(id, index);
  UpdatePrefs();
}

void ToolbarActionsModel::SetVisibleIconCount(size_t count) {
  visible_icon_count_ = (count >= action_ids_.size()) ? -1 : count;

  // Only set the prefs if we're not in highlight mode and the profile is not
  // incognito. Highlight mode is designed to be a transitory state, and should
  // not persist across browser restarts (though it may be re-entered), and we
  // don't store anything in incognito.
  if (!is_highlighting() && !profile_->IsOffTheRecord()) {
    prefs_->SetInteger(extensions::pref_names::kToolbarSize,
                       visible_icon_count_);
  }

  for (Observer& observer : observers_)
    observer.OnToolbarVisibleCountChanged();
}

void ToolbarActionsModel::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  // Notify observers if the extension exists and is in the model.
  if (HasAction(extension_action->extension_id())) {
    for (Observer& observer : observers_)
      observer.OnToolbarActionUpdated(extension_action->extension_id());
  }
}

std::vector<std::unique_ptr<ToolbarActionViewController>>
ToolbarActionsModel::CreateActions(Browser* browser,
                                   ExtensionsContainer* main_bar,
                                   bool in_overflow_mode) {
  DCHECK(browser);
  DCHECK(main_bar);
  std::vector<std::unique_ptr<ToolbarActionViewController>> action_list;

  // action_ids() might not equate to |action_ids_| in the case where a
  // subset is highlighted.
  for (const ActionId& action_id : action_ids()) {
    action_list.push_back(
        CreateActionForId(browser, main_bar, in_overflow_mode, action_id));
  }

  return action_list;
}

std::unique_ptr<ToolbarActionViewController>
ToolbarActionsModel::CreateActionForId(Browser* browser,
                                       ExtensionsContainer* main_bar,
                                       bool in_overflow_mode,
                                       const ActionId& action_id) {
  // We should never have uninitialized actions in action_ids().
  DCHECK(!action_id.empty());
  // Get the extension.
  const extensions::Extension* extension = GetExtensionById(action_id);
  DCHECK(extension);

  // Create and add an ExtensionActionViewController for the extension.
  return std::make_unique<ExtensionActionViewController>(
      extension, browser,
      extension_action_manager_->GetExtensionAction(*extension), main_bar,
      in_overflow_mode);
}

void ToolbarActionsModel::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  // We don't want to add the same extension twice. It may have already been
  // added by EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED below, if the user
  // hides the browser action and then disables and enables the extension.
  if (!HasAction(extension->id()))
    AddExtension(extension);
}

void ToolbarActionsModel::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  bool was_visible_and_has_overflow =
      IsActionVisible(extension->id()) && !all_icons_visible();
  RemoveExtension(extension);
  // If the extension was previously visible and there are overflowed
  // extensions, and this extension is being uninstalled, we reduce the visible
  // count so that we don't pop out a previously-hidden extension.
  if (was_visible_and_has_overflow &&
      reason == extensions::UnloadedExtensionReason::UNINSTALL)
    SetVisibleIconCount(visible_icon_count() - 1);
}

void ToolbarActionsModel::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  // Remove the extension id from the ordered list, if it exists (the extension
  // might not be represented in the list because it might not have an icon).
  RemovePref(extension->id());
}

void ToolbarActionsModel::OnLoadFailure(
    content::BrowserContext* browser_context,
    const base::FilePath& extension_path,
    const std::string& error) {
  for (ToolbarActionsModel::Observer& observer : observers_) {
    observer.OnToolbarActionLoadFailed();
  }
}

void ToolbarActionsModel::RemovePref(const ActionId& action_id) {
  auto pos = std::find(last_known_positions_.begin(),
                       last_known_positions_.end(), action_id);

  if (pos != last_known_positions_.end()) {
    last_known_positions_.erase(pos);
    UpdatePrefs();
  }
}

void ToolbarActionsModel::OnReady() {
  InitializeActionList();

  load_error_reporter_observer_.Add(
      extensions::LoadErrorReporter::GetInstance());

  // Wait until the extension system is ready before observing any further
  // changes so that the toolbar buttons can be shown in their stable ordering
  // taken from prefs.
  extension_registry_observer_.Add(extension_registry_);
  extension_action_observer_.Add(extension_action_api_);

  actions_initialized_ = true;
  for (Observer& observer : observers_)
    observer.OnToolbarModelInitialized();
}

size_t ToolbarActionsModel::FindNewPositionFromLastKnownGood(
    const ActionId& action) {
  // See if we have last known good position for this action.
  size_t new_index = 0;
  // Loop through the ID list of known positions, to count the number of
  // visible action icons preceding |action|'s id.
  for (const ActionId& last_pos_id : last_known_positions_) {
    if (last_pos_id == action)
      return new_index;  // We've found the right position.
    // Found an action, need to see if it is visible.
    for (const ActionId& action_id : action_ids_) {
      if (action_id == last_pos_id) {
        // This extension is visible, update the index value.
        ++new_index;
        break;
      }
    }
  }

  // Position not found.
  return action_ids_.size();
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

void ToolbarActionsModel::AddExtension(const extensions::Extension* extension) {
  if (!ShouldAddExtension(extension))
    return;

  AddAction(extension->id());
}

void ToolbarActionsModel::AddAction(const ActionId& action_id) {
  // We only use AddAction() once the system is initialized.
  CHECK(actions_initialized_);

  // See if we have a last known good position for this extension.
  bool is_new_extension = !base::Contains(last_known_positions_, action_id);

  // New extensions go at the right (end) of the visible extensions. Other
  // extensions go at their previous position.
  size_t new_index = 0;
  if (is_new_extension) {
    new_index = visible_icon_count();
    // For the last-known position, we use the index of the extension that is
    // just before this extension, plus one. (Note that this isn't the same
    // as new_index + 1, because last_known_positions_ can include disabled
    // extensions.)
    int new_last_known_index = new_index == 0
                                   ? 0
                                   : std::find(last_known_positions_.begin(),
                                               last_known_positions_.end(),
                                               action_ids_[new_index - 1]) -
                                         last_known_positions_.begin() + 1;
    // In theory, the extension before this one should always
    // be in last known positions, but if something funny happened with prefs,
    // make sure we handle it.
    // TODO(devlin): Track down these cases so we can CHECK this.
    new_last_known_index =
        std::min<int>(new_last_known_index, last_known_positions_.size());
    last_known_positions_.insert(
        last_known_positions_.begin() + new_last_known_index, action_id);
    UpdatePrefs();
  } else {
    new_index = FindNewPositionFromLastKnownGood(action_id);
  }

  action_ids_.insert(action_ids_.begin() + new_index, action_id);

  // If we're currently highlighting, then even though we add a browser action
  // to the full list (|action_ids_|, there won't be another *visible*
  // browser action, which was what the observers care about.
  if (!is_highlighting()) {
    for (Observer& observer : observers_)
      observer.OnToolbarActionAdded(action_id, new_index);

    int visible_count_delta = 0;
    if (is_new_extension && !all_icons_visible()) {
      // If this is a new extension (and not all extensions are visible), we
      // expand the toolbar out so that the new one can be seen.
      visible_count_delta = 1;
    } else if (profile_->IsOffTheRecord()) {
      // If this is an incognito profile, we also have to check to make sure the
      // overflow matches the main bar's status.
      ToolbarActionsModel* main_model =
          ToolbarActionsModel::Get(profile_->GetOriginalProfile());
      // Find what the index will be in the main bar. Because Observer calls are
      // nondeterministic, we can't just assume the main bar will have the
      // extension and look it up.
      size_t main_index =
          main_model->FindNewPositionFromLastKnownGood(action_id);
      bool visible =
          is_new_extension || main_index < main_model->visible_icon_count();
      // We may need to adjust the visible count if the incognito bar isn't
      // showing all icons and this one is visible, or if it is showing all
      // icons and this is hidden.
      if (visible && !all_icons_visible())
        visible_count_delta = 1;
      else if (!visible && all_icons_visible())
        visible_count_delta = -1;
    }

    if (visible_count_delta)
      SetVisibleIconCount(visible_icon_count() + visible_count_delta);
  }
}

void ToolbarActionsModel::RemoveAction(const ActionId& action_id) {
  auto pos = std::find(action_ids_.begin(), action_ids_.end(), action_id);

  if (pos == action_ids_.end())
    return;

  // If our visible count is set to the current size, we need to decrement it.
  if (visible_icon_count_ == static_cast<int>(action_ids_.size()))
    SetVisibleIconCount(action_ids_.size() - 1);

  action_ids_.erase(pos);

  // If we're in highlight mode, we also have to remove the action from
  // the highlighted list.
  if (is_highlighting()) {
    pos = std::find(highlighted_action_ids_.begin(),
                    highlighted_action_ids_.end(), action_id);
    if (pos != highlighted_action_ids_.end()) {
      highlighted_action_ids_.erase(pos);
      for (Observer& observer : observers_)
        observer.OnToolbarActionRemoved(action_id);
      // If the highlighted list is now empty, we stop highlighting.
      if (highlighted_action_ids_.empty())
        StopHighlighting();
    }
  } else {
    for (Observer& observer : observers_)
      observer.OnToolbarActionRemoved(action_id);
  }

  UpdatePrefs();
}

std::unique_ptr<extensions::ExtensionMessageBubbleController>
ToolbarActionsModel::GetExtensionMessageBubbleController(Browser* browser) {
  std::unique_ptr<extensions::ExtensionMessageBubbleController> controller;
  if (has_active_bubble())
    return controller;
  controller = ExtensionMessageBubbleFactory(browser).GetController();
  if (controller)
    controller->SetIsActiveBubble();
  return controller;
}

bool ToolbarActionsModel::IsActionPinned(const ActionId& action_id) const {
  DCHECK(base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));
  return base::Contains(pinned_action_ids_, action_id);
}

void ToolbarActionsModel::MovePinnedAction(const ActionId& action_id,
                                           size_t target_index) {
  DCHECK(base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));

  auto new_pinned_action_ids = pinned_action_ids_;

  auto current_position = std::find(new_pinned_action_ids.begin(),
                                    new_pinned_action_ids.end(), action_id);
  DCHECK(current_position != new_pinned_action_ids.end());

  const bool move_to_end = size_t{target_index} >= new_pinned_action_ids.size();
  auto target_position =
      move_to_end ? std::prev(new_pinned_action_ids.end())
                  : std::next(new_pinned_action_ids.begin(), target_index);

  // Rotate |action_id| to be in the target position.
  if (target_position < current_position) {
    std::rotate(target_position, current_position, std::next(current_position));
  } else {
    std::rotate(current_position, std::next(current_position),
                std::next(target_position));
  }

  extension_prefs_->SetPinnedExtensions(new_pinned_action_ids);
  // The |pinned_action_ids_| should be updated as a result of updating the
  // preference.
  DCHECK(pinned_action_ids_ == new_pinned_action_ids);
}

void ToolbarActionsModel::RemoveExtension(
    const extensions::Extension* extension) {
  RemoveAction(extension->id());
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

  last_known_positions_ = extension_prefs_->GetToolbarOrder();

  if (profile_->IsOffTheRecord())
    IncognitoPopulate();
  else
    Populate();

  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu))
    pinned_action_ids_ = GetFilteredPinnedActionIds();
}

void ToolbarActionsModel::Populate() {
  DCHECK(!profile_->IsOffTheRecord());

  std::vector<ActionId> all_actions;
  // Ids of actions that have explicit positions.
  std::vector<ActionId> sorted(last_known_positions_.size(), ActionId());
  // Ids of actions that don't have explicit positions.
  std::vector<ActionId> unsorted;

  // Populate the lists.

  // Add the extension action ids to all_actions.
  const extensions::ExtensionSet& extensions =
      extension_registry_->enabled_extensions();
  for (const scoped_refptr<const extensions::Extension>& extension :
       extensions) {
    if (!ShouldAddExtension(extension.get()))
      continue;

    all_actions.push_back(extension->id());
  }

  // Add each action id to the appropriate list. Since the |sorted| list is
  // created with enough room for each id in |positions| (which helps with
  // proper order insertion), holes can be present if there isn't an action
  // for each id. This is handled below when we add the actions to
  // |action_ids_| to ensure that there are never any holes in
  // |action_ids_| itself (or, relatedly, CreateActions()).
  for (const ActionId& action : all_actions) {
    std::vector<ActionId>::const_iterator pos = std::find(
        last_known_positions_.begin(), last_known_positions_.end(), action);
    if (pos != last_known_positions_.end()) {
      sorted[pos - last_known_positions_.begin()] = action;
    } else {
      // Unknown action - push it to the back of unsorted, and add it to the
      // list of ids at the end.
      unsorted.push_back(action);
      last_known_positions_.push_back(action);
    }
  }

  // Merge the lists.
  sorted.insert(sorted.end(), unsorted.begin(), unsorted.end());
  action_ids_.reserve(sorted.size());

  // We don't notify observers of the added extension yet. Rather, observers
  // should wait for the "OnToolbarModelInitialized" notification, and then
  // bulk-update. (This saves a lot of bouncing-back-and-forth here, and allows
  // observers to ensure that the extension system is always initialized before
  // using the extensions).
  for (const ActionId& action : sorted) {
    // Since |sorted| can have holes in it, they will be empty ActionIds.
    // Ignore them.
    if (action.empty())
      continue;

    // It's possible for the extension order to contain actions that aren't
    // actually loaded on this machine.  For example, when extension sync is
    // on, we sync the extension order as-is but double-check with the user
    // before syncing NPAPI-containing extensions, so if one of those is not
    // actually synced, we'll get a NULL in the list.  This sort of case can
    // also happen if some error prevents an extension from loading.
    if (!GetExtensionById(action))
      continue;

    action_ids_.push_back(action);
  }

  // Histogram names are prefixed with "ExtensionToolbarModel" rather than
  // "ToolbarActionsModel" for historical reasons.
  UMA_HISTOGRAM_COUNTS_100("ExtensionToolbarModel.BrowserActionsCount",
                           action_ids_.size());

  if (!action_ids_.empty()) {
    // Visible count can be -1, meaning: 'show all'. Since UMA converts negative
    // values to 0, this would be counted as 'show none' unless we convert it to
    // max.
    UMA_HISTOGRAM_COUNTS_100("ExtensionToolbarModel.BrowserActionsVisible",
                             visible_icon_count_ == -1
                                 ? base::HistogramBase::kSampleType_MAX
                                 : visible_icon_count_);
  }
}

bool ToolbarActionsModel::HasAction(const ActionId& action_id) const {
  return base::Contains(action_ids_, action_id);
}

void ToolbarActionsModel::IncognitoPopulate() {
  DCHECK(profile_->IsOffTheRecord());
  const ToolbarActionsModel* original_model =
      ToolbarActionsModel::Get(profile_->GetOriginalProfile());

  // Find the absolute value of the original model's count.
  int original_visible = original_model->visible_icon_count();

  // In incognito mode, we show only those actions that are incognito-enabled
  // Further, any actions that were overflowed in regular mode are still
  // overflowed. Order is the same as in regular mode.
  visible_icon_count_ = 0;

  for (auto iter = original_model->action_ids_.begin();
       iter != original_model->action_ids_.end(); ++iter) {
    // We should never have an uninitialized action in the model.
    DCHECK(!iter->empty());
    // The extension might not be shown in incognito mode.
    if (!ShouldAddExtension(GetExtensionById(*iter)))
      continue;
    action_ids_.push_back(*iter);
    if (iter - original_model->action_ids_.begin() < original_visible)
      ++visible_icon_count_;
  }
}

void ToolbarActionsModel::UpdatePrefs() {
  if (!extension_prefs_ || profile_->IsOffTheRecord())
    return;

  // Don't observe change caused by self.
  pref_change_registrar_.Remove(extensions::pref_names::kToolbar);
  extension_prefs_->SetToolbarOrder(last_known_positions_);
  pref_change_registrar_.Add(extensions::pref_names::kToolbar,
                             pref_change_callback_);
}

void ToolbarActionsModel::SetActionVisibility(const ActionId& action_id,
                                              bool is_now_visible) {
  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
    DCHECK_NE(is_now_visible, IsActionPinned(action_id));
    auto new_pinned_action_ids = pinned_action_ids_;
    if (is_now_visible) {
      new_pinned_action_ids.push_back(action_id);
    } else {
      base::Erase(new_pinned_action_ids, action_id);
    }
    extension_prefs_->SetPinnedExtensions(new_pinned_action_ids);
    // The |pinned_action_ids_| should be updated as a result of updating the
    // preference.
    DCHECK(pinned_action_ids_ == new_pinned_action_ids);
    return;
  }

  DCHECK(HasAction(action_id));

  int new_size = 0;
  int new_index = 0;
  if (is_now_visible) {
    // If this action used to be hidden, we can't possibly be showing all.
    DCHECK_LT(visible_icon_count(), action_ids_.size());
    // Grow the bar by one and move the action to the end of the visibles.
    new_size = visible_icon_count() + 1;
    new_index = new_size - 1;
  } else {
    // If we're hiding one, we must be showing at least one.
    DCHECK_GE(visible_icon_count(), 0u);
    // Shrink the bar by one and move the action to the beginning of the
    // overflow menu.
    new_size = visible_icon_count() - 1;
    new_index = new_size;
  }
  SetVisibleIconCount(new_size);
  MoveActionIcon(action_id, new_index);
}

void ToolbarActionsModel::OnActionToolbarPrefChange() {
  // If extensions are not ready, defer to later Populate() call.
  if (!actions_initialized_)
    return;

  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
    std::vector<ActionId> pinned_extensions = GetFilteredPinnedActionIds();
    if (pinned_extensions != pinned_action_ids_) {
      pinned_action_ids_ = pinned_extensions;
      for (Observer& observer : observers_)
        observer.OnToolbarPinnedActionsChanged();
    }
  }

  // Recalculate |last_known_positions_| to be |pref_positions| followed by
  // ones that are only in |last_known_positions_|.
  std::vector<ActionId> pref_positions = extension_prefs_->GetToolbarOrder();
  size_t pref_position_size = pref_positions.size();
  for (size_t i = 0; i < last_known_positions_.size(); ++i) {
    if (!base::Contains(pref_positions, last_known_positions_[i])) {
      pref_positions.push_back(last_known_positions_[i]);
    }
  }
  last_known_positions_.swap(pref_positions);

  // Loop over the updated list of last known positions, moving any extensions
  // that are in the wrong place.
  auto desired_pos = action_ids_.begin();
  for (const ActionId& id : last_known_positions_) {
    auto current_pos = std::find_if(
        action_ids_.begin(), action_ids_.end(),
        [&id](const ActionId& action_id) { return action_id == id; });
    if (current_pos == action_ids_.end())
      continue;

    if (current_pos != desired_pos) {
      if (current_pos < desired_pos)
        std::rotate(current_pos, current_pos + 1, desired_pos + 1);
      else
        std::rotate(desired_pos, current_pos, current_pos + 1);
      // Notify the observers to keep them up to date, unless we're highlighting
      // (in which case we're deliberately only showing a subset of actions).
      if (!is_highlighting()) {
        for (Observer& observer : observers_) {
          observer.OnToolbarActionMoved(id, desired_pos - action_ids_.begin());
        }
      }
    }
    ++desired_pos;
  }

  if (last_known_positions_.size() > pref_position_size) {
    // Need to update pref because we have extra icons. But can't call
    // UpdatePrefs() directly within observation closure.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ToolbarActionsModel::UpdatePrefs,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

bool ToolbarActionsModel::HighlightActions(
    const std::vector<ActionId>& ids_to_highlight,
    HighlightType highlight_type) {
  highlighted_action_ids_.clear();

  for (const ActionId& id_to_highlight : ids_to_highlight) {
    for (const ActionId& action_id : action_ids_) {
      if (action_id == id_to_highlight)
        highlighted_action_ids_.push_back(action_id);
    }
  }

  // If we have any actions in |highlighted_action_ids_|, then we entered
  // highlighting mode.
  if (!highlighted_action_ids_.empty()) {
    // It's important that |highlight_type_| is changed immediately before the
    // observers are notified since it changes the result of action_ids().
    highlight_type_ = highlight_type;
    for (Observer& observer : observers_)
      observer.OnToolbarHighlightModeChanged(true);

    // We set the visible icon count after the highlight mode change because
    // the UI actions are created/destroyed during highlight, and doing that
    // prior to changing the size allows us to still have smooth animations.
    if (visible_icon_count() < ids_to_highlight.size())
      SetVisibleIconCount(ids_to_highlight.size());

    return true;
  }

  // Otherwise, we didn't enter highlighting mode (and, in fact, exited it if
  // we were otherwise in it).
  if (is_highlighting())
    StopHighlighting();
  return false;
}

void ToolbarActionsModel::StopHighlighting() {
  if (is_highlighting()) {
    // It's important that |highlight_type_| is changed immediately before the
    // observers are notified since it changes the result of action_ids().
    highlight_type_ = HIGHLIGHT_NONE;
    for (Observer& observer : observers_)
      observer.OnToolbarHighlightModeChanged(false);

    // For the same reason, we don't clear |highlighted_action_ids_| until after
    // the mode changed.
    highlighted_action_ids_.clear();

    // We set the visible icon count after the highlight mode change because
    // the UI actions are created/destroyed during highlight, and doing that
    // prior to changing the size allows us to still have smooth animations.
    int saved_icon_count =
        prefs_->GetInteger(extensions::pref_names::kToolbarSize);
    if (saved_icon_count != visible_icon_count_)
      SetVisibleIconCount(saved_icon_count);
  }
}

const extensions::Extension* ToolbarActionsModel::GetExtensionById(
    const ActionId& action_id) const {
  return extension_registry_->enabled_extensions().GetByID(action_id);
}

bool ToolbarActionsModel::IsActionVisible(const ActionId& action_id) const {
  size_t index = 0u;
  while (action_ids().size() > index && action_ids()[index] != action_id)
    ++index;
  return index < visible_icon_count();
}

std::vector<ToolbarActionsModel::ActionId>
ToolbarActionsModel::GetFilteredPinnedActionIds() const {
  // TODO(pbos): Make sure that the pinned IDs are pruned from ExtensionPrefs on
  // startup so that we don't keep saving stale IDs.
  std::vector<ActionId> filtered_action_ids;
  for (auto& action_id : extension_prefs_->GetPinnedExtensions()) {
    if (HasAction(action_id))
      filtered_action_ids.push_back(action_id);
  }
  return filtered_action_ids;
}
