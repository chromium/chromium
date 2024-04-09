// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_H_

#include <stddef.h>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_management.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"

class Browser;
class PrefService;
class Profile;
class ExtensionsContainer;

namespace extensions {
class ExtensionActionManager;
}  // namespace extensions

// Model for the browser actions toolbar. This is a per-profile instance, and
// manages the user's global preferences.
// Each browser window will attempt to show browser actions as specified by this
// model, but if the window is too narrow, actions may end up pushed into the
// overflow menu on a per-window basis. Callers interested in the arrangement of
// actions in a particular window should check that window's instance of
// ExtensionsContainer, which is responsible for the per-window layout.
class ToolbarActionsModel : public extensions::ExtensionActionAPI::Observer,
                            public extensions::ExtensionRegistryObserver,
                            public extensions::ExtensionManagement::Observer,
                            public extensions::PermissionsManager::Observer,
                            public KeyedService {
 public:
  using ActionId = std::string;

  ToolbarActionsModel(Profile* profile,
                      extensions::ExtensionPrefs* extension_prefs);

  ToolbarActionsModel(const ToolbarActionsModel&) = delete;
  ToolbarActionsModel& operator=(const ToolbarActionsModel&) = delete;

  ~ToolbarActionsModel() override;

  // A class which is informed of changes to the model; represents the view of
  // MVC. Also used for signaling view changes such as showing extension popups.
  // TODO(devlin): Should this really be an observer? It acts more like a
  // delegate.
  class Observer {
   public:
    // Signals that `id` has been added to the toolbar. This will
    // *only* be called after the toolbar model has been initialized.
    virtual void OnToolbarActionAdded(const ActionId& id) = 0;

    // Signals that the given action with `id` has been removed from the
    // toolbar.
    virtual void OnToolbarActionRemoved(const ActionId& id) = 0;

    // Signals that the browser action with `id` has been updated.
    // This method covers lots of different extension updates and could be split
    // in different methods if needed, such as
    // `OnToolbarActionHostPermissionsUpdated`.
    virtual void OnToolbarActionUpdated(const ActionId& id) = 0;

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

  // Returns whether actions can be shown in the toolbar for `browser`.
  static bool CanShowActionsInToolbar(const Browser& browser);

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool actions_initialized() const { return actions_initialized_; }

  const base::flat_set<ActionId>& action_ids() const { return action_ids_; }

  void SetActionVisibility(const ActionId& action_id, bool visible);

  // Returns the extension name corresponding to the `action_id`.
  const std::u16string GetExtensionName(const ActionId& action_id) const;

  // Returns true if `action_id` is in the toolbar model.
  bool HasAction(const ActionId& action_id) const;

  // Returns if `url` is restricted for all extensions with actions in the
  // toolbar.
  bool IsRestrictedUrl(const GURL& url) const;

  // Returns if `url` is a policy-blocked url for all non-enterprise extensions
  // with actions in the toolbar.
  bool IsPolicyBlockedHost(const GURL& url) const;

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

  // extensions::ExtensionManagement::Observer:
  void OnExtensionManagementSettingsChanged() override;

  // extensions::PermissionsManager::Observer:
  void OnExtensionPermissionsUpdated(
      const extensions::Extension& extension,
      const extensions::PermissionSet& permissions,
      extensions::PermissionsManager::UpdateReason reason) override;
  void OnActiveTabPermissionGranted(
      const extensions::Extension& extension) override;

  // KeyedService:
  void Shutdown() override;

  // To be called after the extension service is ready; gets loaded extensions
  // from the ExtensionRegistry, their saved order from the pref service, and
  // constructs |action_ids_| from these data. IncognitoPopulate() takes
  // the shortcut - looking at the regular model's content and modifying it.
  void InitializeActionList();
  void Populate();
  void IncognitoPopulate();

  // Removes any preference for |action_id| and saves the model to prefs.
  void RemovePref(const ActionId& action_id);

  // Returns true if the given |extension| should be added to the toolbar.
  bool ShouldAddExtension(const extensions::Extension* extension);

  // Adds |action_id| to the toolbar.  If the action has an existing preference
  // for toolbar position, that will be used to determine its location.
  // Otherwise it will be placed at the end of the visible actions.
  void AddAction(const ActionId& action_id);

  // Removes |action_id| from the toolbar.
  void RemoveAction(const ActionId& action_id);

  // Looks up and returns the extension with the given |id| in the set of
  // enabled extensions.
  const extensions::Extension* GetExtensionById(const ActionId& id) const;

  // Updates |pinned_action_ids_| per GetFilteredPinnedActionIds() and notifies
  // observers if they have changed.
  void UpdatePinnedActionIds();

  // Gets a list of pinned action ids that only contains that only contains IDs
  // with a corresponding action in the model.
  std::vector<ActionId> GetFilteredPinnedActionIds() const;

  // Notifies `observers_` that `action_id` has been updated.
  void NotifyToolbarActionUpdated(const ActionId& action_id);

  // Our observers.
  base::ObserverList<Observer>::Unchecked observers_;

  // The Profile this toolbar model is for.
  raw_ptr<Profile> profile_;

  raw_ptr<extensions::ExtensionPrefs> extension_prefs_;
  raw_ptr<PrefService> prefs_;

  // The ExtensionActionAPI object, cached for convenience.
  raw_ptr<extensions::ExtensionActionAPI> extension_action_api_;

  // The ExtensionRegistry object, cached for convenience.
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;

  // The ExtensionActionManager, cached for convenience.
  raw_ptr<extensions::ExtensionActionManager> extension_action_manager_;

  // True if we've handled the initial EXTENSIONS_READY notification.
  bool actions_initialized_;

  // Collection of all action IDs (pinned and unpinned).
  base::flat_set<ActionId> action_ids_;

  // Ordered list of pinned action IDs, indicating the order actions should
  // appear on the toolbar.
  std::vector<ActionId> pinned_action_ids_;

  base::ScopedObservation<extensions::ExtensionActionAPI,
                          extensions::ExtensionActionAPI::Observer>
      extension_action_observation_{this};

  // Listen to extension load, unloaded notifications.
  base::ScopedObservation<extensions::ExtensionRegistry,
                          ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // For observing pinned extensions changing.
  PrefChangeRegistrar pref_change_registrar_;

  base::ScopedObservation<extensions::ExtensionManagement,
                          extensions::ExtensionManagement::Observer>
      extension_management_observation_{this};

  base::ScopedObservation<extensions::PermissionsManager,
                          extensions::PermissionsManager::Observer>
      permissions_manager_observation_{this};

  base::WeakPtrFactory<ToolbarActionsModel> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_MODEL_H_
