// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_H_

#include <stddef.h>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

class Browser;
class PrefService;
class Profile;
class ExtensionsContainer;
class ToolbarActionViewController;

namespace extensions {
class ExtensionActionManager;
class ExtensionMessageBubbleController;
}  // namespace extensions

// Model for the browser actions toolbar. This is a per-profile instance, and
// manages the user's global preferences.
// Each browser window will attempt to show browser actions as specified by this
// model, but if the window is too narrow, actions may end up pushed into the
// overflow menu on a per-window basis. Callers interested in the arrangement of
// actions in a particular window should check that window's instance of
// ExtensionsContainer, which is responsible for the per-window layout.
class ToolbarActionsModel : public extensions::ExtensionActionAPI::Observer,
                            public extensions::LoadErrorReporter::Observer,
                            public extensions::ExtensionRegistryObserver,
                            public extensions::ExtensionManagement::Observer,
                            public KeyedService {
 public:
  using ActionId = std::string;

  ToolbarActionsModel(Profile* profile,
                      extensions::ExtensionPrefs* extension_prefs);
  ~ToolbarActionsModel() override;

  // A class which is informed of changes to the model; represents the view of
  // MVC. Also used for signaling view changes such as showing extension popups.
  // TODO(devlin): Should this really be an observer? It acts more like a
  // delegate.
  class Observer {
   public:
    // Signals that |id| has been added to the toolbar at |index|. This will
    // *only* be called after the toolbar model has been initialized.
    virtual void OnToolbarActionAdded(const ActionId& id, int index) = 0;

    // Signals that the given action with |id| has been removed from the
    // toolbar.
    virtual void OnToolbarActionRemoved(const ActionId& id) = 0;

    // Signals that the given action with |id| has been moved to |index|.
    // |index| is the desired *final* index of the action (that is, in the
    // adjusted order, action should be at |index|).
    virtual void OnToolbarActionMoved(const ActionId& id, int index) = 0;

    // Signals that the extension, corresponding to the toolbar action, has
    // failed to load.
    virtual void OnToolbarActionLoadFailed() = 0;

    // Signals that the browser action with |id| has been updated.
    virtual void OnToolbarActionUpdated(const ActionId& id) = 0;

    // Signals when the container needs to be redrawn because of a size change,
    // and when the model has finished loading.
    virtual void OnToolbarVisibleCountChanged() = 0;

    // Signals that the toolbar model has been initialized, so that if any
    // observers were postponing animation during the initialization stage, they
    // can catch up.
    virtual void OnToolbarModelInitialized() = 0;

    // Called whenever the pinned actions change.
    virtual void OnToolbarPinnedActionsChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Convenience function to get the ToolbarActionsModel for a Profile.
  static ToolbarActionsModel* Get(Profile* profile);

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Moves the given action with |id|'s icon to the given |index|.
  void MoveActionIcon(const ActionId& id, size_t index);

  // Sets the number of action icons that should be visible.
  // If count == size(), this will set the visible icon count to -1, meaning
  // "show all actions".
  void SetVisibleIconCount(size_t count);

  // Note that this (and all_icons_visible()) are the global default, but are
  // inappropriate for determining a specific window's state - for that, use
  // the ToolbarActionsBar.
  size_t visible_icon_count() const {
    // We have guards around this because |visible_icon_count_| can be set by
    // prefs/sync, and we want to ensure that the icon count returned is within
    // bounds.
    return visible_icon_count_ == -1
               ? action_ids().size()
               : std::min(static_cast<size_t>(visible_icon_count_),
                          action_ids().size());
  }
  bool all_icons_visible() const {
    return visible_icon_count() == action_ids().size();
  }

  bool actions_initialized() const { return actions_initialized_; }

  std::vector<std::unique_ptr<ToolbarActionViewController>> CreateActions(
      Browser* browser,
      ExtensionsContainer* main_bar,
      bool in_overflow_menu);
  std::unique_ptr<ToolbarActionViewController> CreateActionForId(
      Browser* browser,
      ExtensionsContainer* main_bar,
      bool in_overflow_menu,
      const ActionId& action_id);

  const std::vector<ActionId>& action_ids() const { return action_ids_; }

  bool has_active_bubble() const { return has_active_bubble_; }
  void set_has_active_bubble(bool has_active_bubble) {
    has_active_bubble_ = has_active_bubble;
  }

  void SetActionVisibility(const ActionId& action_id, bool visible);

  void OnActionToolbarPrefChange();

  // Gets the ExtensionMessageBubbleController that should be shown for this
  // profile, if any.
  std::unique_ptr<extensions::ExtensionMessageBubbleController>
  GetExtensionMessageBubbleController(Browser* browser);

  // Returns true if the action is pinned to the toolbar.
  bool IsActionPinned(const ActionId& action_id) const;

  // Returns true if the action is force-pinned to the toolbar.
  bool IsActionForcePinned(const ActionId& action_id) const;

  // Move the pinned action for |action_id| to |target_index|.
  void MovePinnedAction(const ActionId& action_id, size_t target_index);

  // Returns the ordered list of ids of pinned actions.
  const std::vector<ActionId>& pinned_action_ids() const {
    return pinned_action_ids_;
  }

 private:
  // Callback when actions are ready.
  void OnReady();

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // ExtensionActionAPI::Observer:
  void OnExtensionActionUpdated(
      extensions::ExtensionAction* extension_action,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context) override;

  // extensions::LoadErrorReporter::Observer:
  void OnLoadFailure(content::BrowserContext* browser_context,
                     const base::FilePath& extension_path,
                     const std::string& error) override;

  // extensions::ExtensionManagement::Observer:
  void OnExtensionManagementSettingsChanged() override;

  // To be called after the extension service is ready; gets loaded extensions
  // from the ExtensionRegistry, their saved order from the pref service, and
  // constructs |action_ids_| from these data. IncognitoPopulate() takes
  // the shortcut - looking at the regular model's content and modifying it.
  void InitializeActionList();
  void Populate();
  void IncognitoPopulate();

  // Save the model to prefs.
  void UpdatePrefs();

  // Removes any preference for |action_id| and saves the model to prefs.
  void RemovePref(const ActionId& action_id);

  // Finds the last known visible position of the icon for |action|. The value
  // returned is a zero-based index into the vector of visible actions.
  size_t FindNewPositionFromLastKnownGood(const ActionId& action_id);

  // Returns true if the given |extension| should be added to the toolbar.
  bool ShouldAddExtension(const extensions::Extension* extension);

  // Adds or removes the given |extension| from the toolbar model.
  void AddExtension(const extensions::Extension* extension);
  void RemoveExtension(const extensions::Extension* extension);

  // Returns true if |action_id| is in the toolbar model.
  bool HasAction(const ActionId& action_id) const;

  // Adds |action_id| to the toolbar.  If the action has an existing preference
  // for toolbar position, that will be used to determine its location.
  // Otherwise it will be placed at the end of the visible actions.
  void AddAction(const ActionId& action_id);

  // Removes |action_id| from the toolbar.
  void RemoveAction(const ActionId& action_id);

  // Looks up and returns the extension with the given |id| in the set of
  // enabled extensions.
  const extensions::Extension* GetExtensionById(const ActionId& id) const;

  // Returns true if the action is visible on the toolbar.
  bool IsActionVisible(const ActionId& action_id) const;

  // Updates |pinned_action_ids_| per GetFilteredPinnedActionIds() and notifies
  // observers if they have changed.
  void UpdatePinnedActionIds();

  // Gets a list of pinned action ids that only contains that only contains IDs
  // with a corresponding action in the model.
  std::vector<ActionId> GetFilteredPinnedActionIds() const;

  // Our observers.
  base::ObserverList<Observer>::Unchecked observers_;

  // The Profile this toolbar model is for.
  Profile* profile_;

  extensions::ExtensionPrefs* extension_prefs_;
  PrefService* prefs_;

  // The ExtensionActionAPI object, cached for convenience.
  extensions::ExtensionActionAPI* extension_action_api_;

  // The ExtensionRegistry object, cached for convenience.
  extensions::ExtensionRegistry* extension_registry_;

  // The ExtensionActionManager, cached for convenience.
  extensions::ExtensionActionManager* extension_action_manager_;

  // True if we've handled the initial EXTENSIONS_READY notification.
  bool actions_initialized_;

  // Ordered list of browser action IDs.
  std::vector<ActionId> action_ids_;

  // Set of pinned action IDs.
  std::vector<ActionId> pinned_action_ids_;

  // A list of action ids ordered to correspond with their last known
  // positions.
  std::vector<ActionId> last_known_positions_;

  // The number of icons visible (the rest should be hidden in the overflow
  // chevron). A value of -1 indicates that all icons should be visible.
  // Instead of using this variable directly, use visible_icon_count() if
  // possible.
  // TODO(devlin): Make a new variable to indicate that all icons should be
  // visible, instead of overloading this one.
  int visible_icon_count_;

  // Whether or not there is an active ExtensionMessageBubbleController
  // associated with the profile. There should only be one at a time.
  bool has_active_bubble_;

  base::ScopedObservation<extensions::ExtensionActionAPI,
                          extensions::ExtensionActionAPI::Observer>
      extension_action_observation_{this};

  // Listen to extension load, unloaded notifications.
  base::ScopedObservation<extensions::ExtensionRegistry,
                          ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // For observing change of toolbar order preference by external entity (sync).
  PrefChangeRegistrar pref_change_registrar_;
  base::RepeatingClosure pref_change_callback_;

  base::ScopedObservation<extensions::LoadErrorReporter,
                          extensions::LoadErrorReporter::Observer>
      load_error_reporter_observation_{this};

  base::ScopedObservation<extensions::ExtensionManagement,
                          extensions::ExtensionManagement::Observer>
      extension_management_observation_{this};

  base::WeakPtrFactory<ToolbarActionsModel> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_H_
