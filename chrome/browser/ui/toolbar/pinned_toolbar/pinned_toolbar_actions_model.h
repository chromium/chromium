// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_PINNED_TOOLBAR_ACTIONS_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_PINNED_TOOLBAR_ACTIONS_MODEL_H_

#include <string>

#include "base/observer_list.h"
#include "chrome/browser/ui/browser.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/actions/action_id.h"

class Profile;

// Used to keep track of the pinned elements/actions in the toolbar of the
// browser. This is a per-profile instance, and manages the user's pinned action
// preferences.
class PinnedToolbarActionsModel : public KeyedService {
 public:
  explicit PinnedToolbarActionsModel(Profile* profile);

  PinnedToolbarActionsModel(const PinnedToolbarActionsModel&) = delete;
  PinnedToolbarActionsModel& operator=(const PinnedToolbarActionsModel&) =
      delete;

  ~PinnedToolbarActionsModel() override;

  // Used to notify objects that extend this class that a change has occurred in
  // the model. Note that added/removed/moved are NOT called when the pref is
  // updated directly, e.g. for changes synced from another device.
  class Observer {
   public:
    // Signals that `id` has been added to the model. This will
    // *only* be called after the model has been initialized. N.B. Direct pref
    // updates which happen to add an action WILL NOT call this method.
    virtual void OnActionAddedLocally(const actions::ActionId& id) = 0;

    // Signals that the given action with `id` has been removed from the
    // model. N.B. Direct pref updates which happen to remove an action WILL NOT
    // call this method.
    virtual void OnActionRemovedLocally(const actions::ActionId& id) = 0;

    // Signals that the given action with `id` has been moved in the model.
    // N.B. Direct pref updates which happen to move an action WILL NOT call
    // this method.
    virtual void OnActionMovedLocally(const actions::ActionId& id,
                                      int from_index,
                                      int to_index) = 0;

    // Called when the pinned actions change, in any way for any reason. Unlike
    // the above methods, this does include pref updates.
    virtual void OnActionsChanged() = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Convenience function to get the PinnedToolbarActionsModel for a Profile.
  static PinnedToolbarActionsModel* Get(Profile* profile);

  // Adds or removes an observer.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Verify if we can update the model with the current profile.
  bool CanUpdate();

  // Returns true if `action_id` is in the toolbar model.
  bool Contains(const actions::ActionId& action_id) const;

  // Move the pinned action for |action_id| to |target_index|.
  void MovePinnedAction(const actions::ActionId& action_id, int target_index);

  // Updates the Action state of `action_id`.
  // 1) Adds `action_id` to the model if `should_pin` is true and the id does
  // not exist in the model.
  // 2) Removes `action_id` from the model if
  // `should_pin` is false and the id exists in the model.
  virtual void UpdatePinnedState(const actions::ActionId& action_id,
                                 const bool should_pin);

  // TODO(b/307350981): Remove MaybeUpdateSearchCompanionPinnedState() and
  // UpdateSearchCompanionDefaultState() after migration is complete.
  // Migrate the search companion pin state
  // `kSidePanelCompanionEntryPinnedToToolbar` into kPinnedActions.
  void MaybeUpdateSearchCompanionPinnedState(
      bool companion_should_be_default_pinned);

  // Resets the pinned actions to default. NOTE: This also affects the home and
  // forward buttons, even though those are not otherwise managed by this model.
  virtual void ResetToDefault();

  // Returns true if the set of pinned actions is the default set. NOTE: This
  // also includes the home and forward buttons, even though those are not
  // otherwise managed by this model.
  bool IsDefault() const;

  // TODO(b/353323253): Remove after Pinned Chrome Labs migration is complete.
  void MaybeMigrateChromeLabsPinnedState();

  // Returns the ordered list of pinned ActionIds.
  virtual const std::vector<actions::ActionId>& PinnedActionIds() const;

 private:
  // Adds the `action_id` to the kPinnedActions pref.
  void PinAction(const actions::ActionId& action_id);

  // Removes the `action_id` from the kPinnedActions pref.
  void UnpinAction(const actions::ActionId& action_id);

  // Called when the kPinnedActions pref is changed. |pinned_action_ids_| is
  // replaced with the entries in the kPinnedActions pref object. Should
  // maintain insertion order. Notify observers the model has been updated with
  // the latest data from the pref.
  void UpdatePinnedActionIds();

  // Calculates and updates `kPinnedActions` with current default pinned state
  // of the search companion feature.
  void UpdateSearchCompanionDefaultState(
      bool companion_should_be_default_pinned);

  void UpdatePref(const std::vector<actions::ActionId>& updated_list);

  // Our observers.
  base::ObserverList<Observer>::Unchecked observers_;

  raw_ptr<Profile> profile_;

  // Used to retrieve and update the prefs object storing the currently pinned
  // actions.
  raw_ptr<PrefService> pref_service_;

  // For observing changes to the pinned actions.
  PrefChangeRegistrar pref_change_registrar_;

  // Ordered list of pinned action IDs which will be displayed in the toolbar.
  std::vector<actions::ActionId> pinned_action_ids_;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_PINNED_TOOLBAR_ACTIONS_MODEL_H_
