// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/base/models/image_model.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class WebContents;
}  // namespace content

class BrowserWindowInterface;

// The platform agnostic model for the extensions menu.
class ExtensionsMenuViewModel : public extensions::PermissionsManager::Observer,
                                public ToolbarActionsModel::Observer,
                                public TabListInterfaceObserver,
                                public content::WebContentsObserver {
 public:
  // Defines the delegate interface for retrieving platform-specific
  // information.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Creates the platform-agnostic action view model for the given
    // `extension_id`. ExtensionsMenuViewModel will own the returned object, but
    // the Delegate is responsible for constructing it with the necessary
    // platform dependencies.
    virtual std::unique_ptr<ExtensionActionViewModel> CreateActionViewModel(
        const extensions::ExtensionId& extension_id) = 0;
  };

  // Observer used to notify platforms about changes to the model.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies the delegate that the active web contents changed, which may
    // have impacted the model's content (e.g host access requests may have
    // changed).
    virtual void OnPageNavigation() = 0;

    // Notifies the delegate that a new host access request was added
    // with `extension_id` on `index`.
    virtual void OnHostAccessRequestAdded(
        const extensions::ExtensionId& extension_id,
        int index) = 0;

    // Notifies the delegate that host access request with `extension_id` was
    // updates on `index`.
    virtual void OnHostAccessRequestUpdated(
        const extensions::ExtensionId& extension_id,
        int index) = 0;

    // Notifies the delegate that the host access request for
    // `extension_id` on `index` was removed.
    virtual void OnHostAccessRequestRemoved(
        const extensions::ExtensionId& extension_id,
        int index) = 0;

    // Notifies the delegate that host access requests on the current site were
    // cleared.
    virtual void OnHostAccessRequestsCleared() = 0;

    virtual void OnShowHostAccessRequestsInToolbarChanged(
        const extensions::ExtensionId& extension_id,
        bool can_show_requests) = 0;

    // Called when an action is added to the menu model at `index`.
    virtual void OnActionAdded(ExtensionActionViewModel* action_model,
                               int index) = 0;

    // Called when an action is removed from the menu model at `index`.
    virtual void OnActionRemoved(const ToolbarActionsModel::ActionId& action_id,
                                 int index) = 0;

    // Called when an action is updated in the menu model. This doesn't cover
    // icon updates because (a) icons are loaded asynchronously and (b) they
    // only require updating the icon and no other fields (e.g an action update
    // can include a permissions change which affects other views apart from the
    // action menu entry).
    virtual void OnActionUpdated(
        const ToolbarActionsModel::ActionId& action_id) = 0;

    // Called when an action icon is updated.
    virtual void OnActionIconUpdated(
        const ToolbarActionsModel::ActionId& action_id) = 0;

    // Called after all actions are added in the menu model after menu model
    // construction.
    virtual void OnActionsInitialized() = 0;

    // Notifies the delegate that the pinned toolbar actions have changed
    virtual void OnToolbarPinnedActionsChanged() = 0;

    // Notifies the delegate that the user permissions settings changed on the
    // current site.
    virtual void OnUserPermissionsSettingsChanged() = 0;
  };

  // The type of optional section to display in the menu.
  enum class OptionalSection {
    // A section alerting the user that a page reload is required for changes to
    // take effect.
    kReloadPage,
    // A section listing extensions that have host access requests to the
    // current
    // site.
    kHostAccessRequests,
    // No optional section should be displayed.
    kNone
  };

  // A generic structure for UI controls (buttons, toggles, radio buttons).
  // This struct is mirrored in Java (ExtensionsMenuTypes.java).
  struct ControlState {
    // Represents the availability and interactivity the control.
    enum class Status {
      // The control is not displayed.
      kHidden,
      // The control is displayed but cannot be interacted with.
      kDisabled,
      // The control is displayed and interactive.
      kEnabled
    };

    ControlState();
    ControlState(const ControlState&);
    ControlState& operator=(const ControlState&);
    ~ControlState();

    // The interactivity status of the control.
    Status status = Status::kHidden;
    // The text label to display. Empty if not applicable.
    std::u16string text;
    // The accessible name. Empty if not applicable.
    std::u16string accessible_name;
    // The tooltip text label. Empty if not applicable.
    std::u16string tooltip_text;
    // The checked/toggled state. False for buttons with no on/off state.
    bool is_on = false;
    // The icon for the control. Empty if not applicable.
    ui::ImageModel icon;
  };

  // Hold the information for an extension's host access request.
  struct HostAccessRequest {
    // The if of the extension.
    extensions::ExtensionId extension_id;
    // The display name for the extension.
    std::u16string extension_name;
    // The display icon for the extension.
    ui::ImageModel extension_icon;
  };

  // Holds the information for an extension's site permissions in the extensions
  // menu. This will be used by the platform delegate as needed.
  struct ExtensionSitePermissionsState {
    ExtensionSitePermissionsState();
    ExtensionSitePermissionsState(const ExtensionSitePermissionsState&);
    ExtensionSitePermissionsState& operator=(
        const ExtensionSitePermissionsState&);
    ~ExtensionSitePermissionsState();

    // The display name for the extension.
    std::u16string extension_name;
    // THe display icon for the extension.
    ui::ImageModel extension_icon;
    // The state for the 'on click' site access option.
    ControlState on_click_option;
    // The state for the 'on site' site access option.
    ControlState on_site_option;
    // The state for the 'on all sites' site access option.
    ControlState on_all_sites_option;
    // The state for the 'show requests' toggle.
    ControlState show_requests_toggle;
  };

  // Holds the information for the site settings in the extension's menu. This
  // will be used by the platform delegate as needed.
  struct SiteSettingsState {
    // The resource ID for the text label.
    std::u16string label;
    // Whether to show a tooltip explaining why the setting is in its current
    // state (e.g. if controlled by enterprise policy).
    bool has_tooltip;
    // The state of the site settings toggle.
    ControlState toggle;
  };

  // Holds the information about how the extension's menu entry should look
  // like. This will be used by the platform delegate as needed.
  // This struct is mirrored in Java (ExtensionsMenuTypes.java).
  struct MenuEntryState {
    MenuEntryState();
    MenuEntryState(const MenuEntryState&);
    MenuEntryState& operator=(const MenuEntryState&);
    ~MenuEntryState();

    // The id of the extension in the menu entry.
    extensions::ExtensionId extension_id;
    // The state for the action button.
    ControlState action_button;
    // The state for the context menu button.
    ControlState context_menu_button;
    // The state for the site access toggle.
    ControlState site_access_toggle;
    // The state for the site permissions button.
    ControlState site_permissions_button;
    // Whether the extension is installed from an enterprise policy.
    bool is_enterprise;
  };

  ExtensionsMenuViewModel(BrowserWindowInterface* browser, Delegate* delegate);
  ExtensionsMenuViewModel(const ExtensionsMenuViewModel&) = delete;
  const ExtensionsMenuViewModel& operator=(const ExtensionsMenuViewModel&) =
      delete;
  ~ExtensionsMenuViewModel() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Updates the extension's site access for the current site.
  void UpdateSiteAccess(
      const extensions::ExtensionId& extension_id,
      extensions::PermissionsManager::UserSiteAccess site_access);

  // Allows the extension's host access request to the current site.
  void AllowHostAccessRequest(const extensions::ExtensionId& extension_id);

  // Dismisses the extension's host access request to the current site.
  void DismissHostAccessRequest(const extensions::ExtensionId& extension_id);

  // Sets whether the extension can show host access requests in the toolbar.
  void ShowHostAccessRequestsInToolbar(
      const extensions::ExtensionId& extension_id,
      bool show);

  // Grants the extension site access to the current site.
  void GrantSiteAccess(const extensions::ExtensionId& extension_id);

  // Revokes the extension's site access from the current site.
  void RevokeSiteAccess(const extensions::ExtensionId& extension_id);

  // Update the site setting's for the current site.
  void UpdateSiteSetting(
      extensions::PermissionsManager::UserSiteSetting site_setting);

  // Executes the primary action for the extension with `extension_id`.
  void ExecuteAction(const extensions::ExtensionId& extension_id);

  // Reloads the current web contents.
  void ReloadWebContents();

  // Returns true if the site permissions page can be shown for the given
  // `extension_id`.
  bool CanShowSitePermissionsPage(const extensions::ExtensionId& extension_id);

  // Returns the action button state for an extension's menu entry.
  ControlState GetActionButtonState(const extensions::ExtensionId& extension_id,
                                    const gfx::Size& icon_size);

  // Returns the icon for an extension's action at `action_index`.
  ui::ImageModel GetActionIcon(int action_index, const gfx::Size& icon_size);

  // Returns the state for the extension's context menu button.
  ControlState GetContextMenuButtonState(
      const extensions::ExtensionId& extension_id);
  ControlState GetContextMenuButtonState(
      ExtensionActionViewModel* action_model);

  // Returns the host access request information for an extension.
  HostAccessRequest GetHostAccessRequest(
      const extensions::ExtensionId& extension_id,
      const gfx::Size& icon_size);

  // Returns the site access permissions state for an extension. This will crash
  // if called when the user cannot modify the extension site permissions, as
  // this method would compute invalid values.
  ExtensionSitePermissionsState GetExtensionSitePermissionsState(
      const extensions::ExtensionId& extension_id,
      const gfx::Size& icon_size);

  // Returns the show requests toggle state for an extension.
  ControlState GetExtensionShowRequestsToggleState(
      const extensions::ExtensionId& extension_id);

  // Returns the menu item state for an extension. `action_icon_size` is the
  // size the extension icon should have.
  MenuEntryState GetMenuEntryState(const extensions::ExtensionId& extension_id,
                                   const gfx::Size& action_icon_size);

  // Returns the optional section to display in the menu.
  OptionalSection GetOptionalSection();

  // Returns the site settings for the current web contents.
  SiteSettingsState GetSiteSettingsState();

  // Returns a read-only reference to the list of sorted action view models.
  const std::vector<std::unique_ptr<ExtensionActionViewModel>>&
  action_models() {
    return action_models_;
  }

  // Returns the id's of the extensions that have valid host access requests for
  // the current site.
  const std::vector<extensions::ExtensionId>& host_access_requests() {
    return host_access_requests_;
  }

  // Returns whether the view model has been populated after action models were
  // initialized.
  bool is_populated() { return is_populated_; }

  // PermissionsManager::Observer:
  void OnHostAccessRequestAdded(const extensions::ExtensionId& extension_id,
                                int tab_id) override;
  void OnHostAccessRequestUpdated(const extensions::ExtensionId& extension_id,
                                  int tab_id) override;
  void OnHostAccessRequestRemoved(const extensions::ExtensionId& extension_id,
                                  int tab_id) override;
  void OnHostAccessRequestsCleared(int tab_id) override;
  void OnHostAccessRequestDismissedByUser(
      const extensions::ExtensionId& extension_id,
      const url::Origin& origin) override;
  void OnShowAccessRequestsInToolbarChanged(
      const extensions::ExtensionId& extension_id,
      bool can_show_requests) override;
  void OnUserPermissionsSettingsChanged(
      const extensions::PermissionsManager::UserPermissionsSettings& settings)
      override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

  // TabListInterfaceObserver:
  // Sometimes, menu can stay open when tab changes (e.g keyboard shortcuts) or
  // due to the extension (e.g extension switching the active tab). Thus, we
  // listen for active tab changes to properly update the menu content.
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  void OnTabListDestroyed(TabListInterface& tab_list) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

 private:
  // Populates `action_models_` and `host_access_requests_` after actions
  // have been initialized.
  void Populate();

  // Adds `extension_id` to `host_access_requests` in the correct sorted
  // order and notifies observers.
  void AddHostAccessRequest(const extensions::ExtensionId& extension_id);

  // Removes `extension_id` from `host_access_requests` and notifies
  // observers.
  void RemoveHostAccessRequest(const extensions::ExtensionId& extension_id);

  // Updates `host_access_requests_` with the extensions that have active host
  // access requests, clearing any existent ones. This should be called when
  // actions are initialized, or on page navigations.
  void UpdateHostAccessRequests();

  // Returns the extension action view model for the given `extension_id`.
  ExtensionActionViewModel* GetActionViewModel(
      const extensions::ExtensionId& extension_id) const;

  // Callback for when an icon in `action_models_` updates.
  void OnActionIconUpdated(const extensions::ExtensionId& extension_id);

  // Updates the model when web contents changed, and notifies observers.
  void OnWebContentsChanged(content::WebContents* web_contents);

  content::WebContents* GetActiveWebContents();

  // The browser window that the extensions menu is in.
  raw_ptr<BrowserWindowInterface> browser_;

  // The observers that handles platform-specific UI.
  base::ObserverList<Observer> observers_;

  // The delegate to retrieve platform-specific information.
  raw_ptr<Delegate> delegate_;

  // Whether the view model has been populated after action models were
  // initialized.
  bool is_populated_ = false;

  // The actions models ordered alphabetically by their action name.
  std::vector<std::unique_ptr<ExtensionActionViewModel>> action_models_;

  // Map of action IDs to their respective `ExtensionActionViewModel` update
  // subscriptions for icon updates.
  std::map<ToolbarActionsModel::ActionId, base::CallbackListSubscription>
      action_icon_subscriptions_;

  // The extensions that have valid host access requests on the current site.
  std::vector<extensions::ExtensionId> host_access_requests_;

  base::ScopedObservation<extensions::PermissionsManager,
                          extensions::PermissionsManager::Observer>
      permissions_manager_observation_{this};

  const raw_ptr<ToolbarActionsModel> toolbar_model_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      toolbar_model_observation_{this};

  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_interface_observation_{this};
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_
