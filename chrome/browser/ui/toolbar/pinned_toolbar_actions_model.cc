// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model.h"

#include <iterator>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"

PinnedToolbarActionsModel::PinnedToolbarActionsModel(Profile* profile)
    : profile_(profile), pref_service_(profile_->GetPrefs()) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kPinnedActions,
      base::BindRepeating(&PinnedToolbarActionsModel::UpdatePinnedActionIds,
                          base::Unretained(this)));

  // Initialize the model with the current state of the kPinnedActions pref.
  UpdatePinnedActionIds();

  // TODO(b/307350981): Remove when migration is complete.
  MaybeMigrateSearchCompanionPinnedState();
}

PinnedToolbarActionsModel::~PinnedToolbarActionsModel() = default;

// static
PinnedToolbarActionsModel* PinnedToolbarActionsModel::Get(Profile* profile) {
  return PinnedToolbarActionsModelFactory::GetForProfile(profile);
}

void PinnedToolbarActionsModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PinnedToolbarActionsModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PinnedToolbarActionsModel::CanUpdate() {
  // TODO(dljames/corising): Update this function as needed with other guards /
  // requirements as they come up.

  // At a minimum, incognito should be read-only. Guest mode should not be
  // able to modify the prefs either.
  return profile_->IsRegularProfile();
}

bool PinnedToolbarActionsModel::Contains(
    const actions::ActionId& action_id) const {
  auto iter = base::ranges::find(pinned_action_ids_, action_id);
  return iter != pinned_action_ids_.end();
}

void PinnedToolbarActionsModel::UpdatePinnedState(
    const actions::ActionId& action_id,
    const bool should_pin) {
  if (!CanUpdate()) {
    // At a minimum, incognito should be read-only. Guest mode should not be
    // able to modify the prefs either.
    return;
  }

  if (action_id == kActionSidePanelShowSearchCompanion) {
    pref_service_->SetBoolean(prefs::kPinnedSearchCompanionMigrationComplete,
                              true);
  }

  const bool is_pinned = Contains(action_id);
  if (!is_pinned && should_pin) {
    PinAction(action_id);
  } else if (is_pinned && !should_pin) {
    UnpinAction(action_id);
  }
}

void PinnedToolbarActionsModel::MovePinnedAction(
    const actions::ActionId& action_id,
    int target_index) {
  if (!CanUpdate()) {
    // At a minimum, incognito should be read-only. Guest mode should not be
    // able to modify the prefs either.
    return;
  }

  if (target_index < 0 || target_index >= int(pinned_action_ids_.size())) {
    // Do nothing if the target index is out of bounds.
    return;
  }

  auto action_to_move = base::ranges::find(pinned_action_ids_, action_id);
  if (action_to_move == pinned_action_ids_.end()) {
    // Do nothing if this action is not pinned.
    return;
  }
  // If the target index and starting index are the same, do nothing.
  int start_index = action_to_move - pinned_action_ids_.begin();
  if (start_index == target_index) {
    return;
  }

  std::vector<actions::ActionId> updated_pinned_action_ids = pinned_action_ids_;

  auto start_iter = base::ranges::find(updated_pinned_action_ids, action_id);
  CHECK(start_iter != updated_pinned_action_ids.end());

  auto end_iter = base::ranges::find(updated_pinned_action_ids,
                                     pinned_action_ids_[target_index]);
  CHECK(end_iter != updated_pinned_action_ids.end());

  // Rotate |action_id| to be in the target position.
  bool is_left_to_right_move = target_index > start_index;
  if (is_left_to_right_move) {
    std::rotate(start_iter, std::next(start_iter), std::next(end_iter));
  } else {
    std::rotate(end_iter, start_iter, std::next(start_iter));
  }

  // Updating the pref causes `UpdatePinnedActionIds()` to be called.
  UpdatePref(updated_pinned_action_ids);

  // Notify observers the action was moved.
  for (Observer& observer : observers_) {
    observer.OnActionMoved(action_id, start_index, target_index);
  }
}

void PinnedToolbarActionsModel::PinAction(const actions::ActionId& action_id) {
  std::vector<actions::ActionId> updated_pinned_action_ids = pinned_action_ids_;

  updated_pinned_action_ids.push_back(action_id);

  // Updating the pref causes `UpdatePinnedActionIds()` to be called.
  UpdatePref(updated_pinned_action_ids);

  // Notify observers the action was added.
  for (Observer& observer : observers_) {
    observer.OnActionAdded(action_id);
  }
}

void PinnedToolbarActionsModel::UnpinAction(
    const actions::ActionId& action_id) {
  std::vector<actions::ActionId> updated_pinned_action_ids = pinned_action_ids_;
  updated_pinned_action_ids.erase(
      std::remove_if(
          updated_pinned_action_ids.begin(), updated_pinned_action_ids.end(),
          [action_id](const actions::ActionId id) { return id == action_id; }),
      updated_pinned_action_ids.end());

  // Updating the pref causes `UpdatePinnedActionIds()` to be called.
  UpdatePref(std::move(updated_pinned_action_ids));

  // Notify observers the action was removed.
  for (Observer& observer : observers_) {
    observer.OnActionRemoved(action_id);
  }
}

void PinnedToolbarActionsModel::UpdatePinnedActionIds() {
  const base::Value::List& updated_pinned_action_ids =
      pref_service_->GetList(prefs::kPinnedActions);

  // TODO(dljames): Investigate if there is a more optimal way to do this kind
  // of conflict resolution. Ideally, we should only react to changes that did
  // not occur directly on the model (Ex: Updates from sync). This would
  // eliminate double processing.
  pinned_action_ids_.clear();
  pinned_action_ids_.reserve(updated_pinned_action_ids.size());

  for (const base::Value& action_id : updated_pinned_action_ids) {
    if (action_id.is_string()) {
      const absl::optional<actions::ActionId>& id =
          actions::ActionIdMap::StringToActionId(action_id.GetString());
      // It could be possible that an ActionId is not mapped to the string if it
      // comes from the prefs object. Example: One version could have an id that
      // another one doesn't.
      if (!id.has_value()) {
        LOG(WARNING)
            << "The following action id does not have a string equivalent: "
            << action_id
            << ". This can happen when different versions of the ActionId are "
               "added to the prefs object.";
        continue;
      }
      pinned_action_ids_.push_back(id.value());
    }
  }

  for (Observer& observer : observers_) {
    observer.OnActionsChanged();
  }
}

void PinnedToolbarActionsModel::MaybeMigrateSearchCompanionPinnedState() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Checks if the search companion action id is present beceause in tests this
  // model can be created before the browser actions are initialized if testing
  // factories are added to create this model. This prevents failures when the
  // companion feature is enabled.
  if (pref_service_->GetBoolean(
          prefs::kPinnedSearchCompanionMigrationComplete) ||
      !CanUpdate() ||
      !actions::ActionIdMap::ActionIdToString(
           kActionSidePanelShowSearchCompanion)
           .has_value()) {
    return;
  }

  if (!companion::IsCompanionFeatureEnabled()) {
    // prefs::kSidePanelCompanionEntryPinnedToToolbar is not registered when
    // companion is disabled.
    return;
  }

  if (!pref_service_->GetUserPrefValue(
          prefs::kSidePanelCompanionEntryPinnedToToolbar)) {
    UpdateSearchCompanionDefaultState();
    return;
  }

  UpdatePinnedState(kActionSidePanelShowSearchCompanion,
                    /*should_pin=*/pref_service_->GetBoolean(
                        prefs::kSidePanelCompanionEntryPinnedToToolbar));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void PinnedToolbarActionsModel::UpdateSearchCompanionDefaultState() {
  // TODO(dljames): Move search companion booleans into helper function for
  // search companion and this class to use.
  bool observed_exps_nav =
      base::FeatureList::IsEnabled(
          companion::features::internal::
              kCompanionEnabledByObservingExpsNavigations) &&
      pref_service_->GetBoolean(companion::kHasNavigatedToExpsSuccessPage);

  bool companion_should_be_default_pinned =
      base::FeatureList::IsEnabled(
          features::kSidePanelCompanionDefaultPinned) ||
      pref_service_->GetBoolean(companion::kExpsOptInStatusGrantedPref) ||
      observed_exps_nav;

  bool is_valid_pin = !Contains(kActionSidePanelShowSearchCompanion) &&
                      companion_should_be_default_pinned;
  bool is_valid_unpin = Contains(kActionSidePanelShowSearchCompanion) &&
                        !companion_should_be_default_pinned;

  std::vector<actions::ActionId> updated_pinned_action_ids = pinned_action_ids_;
  const absl::optional<std::string>& id =
      actions::ActionIdMap::ActionIdToString(
          kActionSidePanelShowSearchCompanion);
  // The ActionId should have a string equivalent.
  CHECK(id.has_value());

  if (is_valid_pin) {
    updated_pinned_action_ids.push_back(kActionSidePanelShowSearchCompanion);
  } else if (is_valid_unpin) {
    updated_pinned_action_ids.erase(
        std::remove_if(updated_pinned_action_ids.begin(),
                       updated_pinned_action_ids.end(),
                       [](const actions::ActionId id) {
                         return id == kActionSidePanelShowSearchCompanion;
                       }),
        updated_pinned_action_ids.end());
  }

  // Updating the pref causes `UpdatePinnedActionIds()` to be called.
  UpdatePref(updated_pinned_action_ids);
}

void PinnedToolbarActionsModel::
    MaybeMigrateSearchCompanionPinnedStateForTesting() {
  MaybeMigrateSearchCompanionPinnedState();
}

void PinnedToolbarActionsModel::UpdatePref(
    const std::vector<actions::ActionId>& updated_list) {
  ScopedListPrefUpdate update(pref_service_, prefs::kPinnedActions);
  base::Value::List& list_of_values = update.Get();
  list_of_values.clear();
  for (auto id : updated_list) {
    const absl::optional<std::string>& id_string =
        actions::ActionIdMap::ActionIdToString(id);
    // The ActionId should have a string equivalent.
    CHECK(id_string.has_value());
    list_of_values.Append(id_string.value());
  }
}
