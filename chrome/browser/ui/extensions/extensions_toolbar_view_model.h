// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_TOOLBAR_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_TOOLBAR_VIEW_MODEL_H_

#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/extensions/extension_action_delegate.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class TabListInterface;

// ViewModel for the ExtensionsToolbarDesktop. This class manages the business
// logic for the order and state of extension actions in the toolbar. It serves
// as the single source of truth for the ordering of the list of actions.
class ExtensionsToolbarViewModel
    : public ExtensionsContainer,
      public ToolbarActionsModel::Observer,
      public content::WebContentsObserver,
      public TabListInterfaceObserver,
      public extensions::PermissionsManager::Observer {
 public:
  // Delegate used to retrieve platform-specific information.
  class Delegate {
   public:
    // Creates the platform-specific action view model.
    virtual std::unique_ptr<ExtensionActionViewModel> CreateActionViewModel(
        const ToolbarActionsModel::ActionId& action_id,
        ExtensionsContainer* extensions_container) = 0;
    // Hides any actively showing popups.
    // TODO(crbug.com/473701535): Determine whether this method belongs in the
    // delegate or the observer.
    virtual void HideActivePopup() = 0;

    // Closes the overflow menu, if it was open. Returns whether or not the
    // overflow menu was closed.
    virtual bool CloseOverflowMenuIfOpen() = 0;

    // Returns whether a popup can be shown.
    virtual bool CanShowToolbarActionPopupForAPICall(
        const ToolbarActionsModel::ActionId& action_id) = 0;

    // Toggle the Extensions menu (as if the user clicked the puzzle piece
    // icon).
    // TODO(crbug.com/473701535): Determine whether this method belongs in the
    // delegate or the observer.
    virtual void ToggleExtensionsMenu() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Observer used to notify platforms about changes to the model.
  class Observer : public base::CheckedObserver {
   public:
    // Called after all actions are added to the model.
    virtual void OnActionsInitialized() = 0;

    // Called when an action is added to the model.
    virtual void OnActionAdded(
        const ToolbarActionsModel::ActionId& action_id) = 0;

    // Called when an action is removed from the model.
    virtual void OnActionRemoved(
        const ToolbarActionsModel::ActionId& action_id) = 0;

    // Called when an action in the model is updated.
    virtual void OnActionUpdated(
        const ToolbarActionsModel::ActionId& action_id) = 0;

    // Called when the pinned actions in the model are changed.
    virtual void OnPinnedActionsChanged() = 0;

    // Called when the active WebContents is changed (e.g. tab change or page
    // navigation). `is_same_document` is true if the change was due to a
    // same-document navigation.
    virtual void OnActiveWebContentsChanged(bool is_same_document) = 0;

    // Called when the extensions that should be displayed in the request
    // access button to be recomputed.
    virtual void OnRequestAccessButtonParamsChanged(
        content::WebContents* web_contents) {}

    // Called when both the extensions button and the request access button
    // should be updated.
    virtual void OnToolbarControlStateUpdated() {}
  };

  enum class ExtensionsToolbarButtonState {
    // All extensions have blocked access to the current site.
    kAllExtensionsBlocked,
    // At least one extension has access to the current site.
    kAnyExtensionHasAccess,
    kDefault,
  };

  // Holds the information for the request access button.
  struct RequestAccessButtonParams {
    RequestAccessButtonParams();
    RequestAccessButtonParams(RequestAccessButtonParams&&);
    RequestAccessButtonParams& operator=(RequestAccessButtonParams&&);
    ~RequestAccessButtonParams();

    std::vector<extensions::ExtensionId> extension_ids;
    std::u16string tooltip_text;
  };

  ExtensionsToolbarViewModel(Delegate* delegate,
                             BrowserWindowInterface* browser,
                             ToolbarActionsModel* actions_model);
  ExtensionsToolbarViewModel(const ExtensionsToolbarViewModel&) = delete;
  ExtensionsToolbarViewModel& operator=(ExtensionsToolbarViewModel&) = delete;
  ~ExtensionsToolbarViewModel() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the view model of the action if it exists, else a nullptr.
  ToolbarActionViewModel* GetActionModelForId(
      const ToolbarActionsModel::ActionId& action_id) const;

  // Move the pinned action `action_id` to `target_index`.
  void MovePinnedAction(const ToolbarActionsModel::ActionId& action_id,
                        size_t target_index);

  // Move this pinned action `action_id` by the specified `move_by` amount.
  void MovePinnedActionBy(const std::string& action_id, int move_by);

  // Returns the sorted list of the IDs of all installed actions.
  const base::flat_set<ToolbarActionsModel::ActionId>& GetAllActionIds() const;

  // Returns the ordered list of ids of pinned actions.
  const std::vector<ToolbarActionsModel::ActionId>& GetPinnedActionIds() const;

  // Returns whether the actions are initialized.
  bool AreActionsInitialized();

  // Returns the state of the extensions toolbar button based on 'web_contents'.
  ExtensionsToolbarButtonState GetButtonState(
      content::WebContents& web_contents) const;

  // Executes the default behavior associated with the action. This should only
  // be called as a result of a user action.
  void ExecuteUserAction(const ToolbarActionsModel::ActionId& action_id,
                         ToolbarActionViewModel::InvocationSource source);

  // Returns RequestAccessButtonParams which contain information to be used in
  // the button's tooltip.
  RequestAccessButtonParams GetRequestAccessButtonParams(
      content::WebContents* web_contents) const;

  // ExtensionsContainer:
  ToolbarActionViewModel* GetActionForId(const std::string& action_id) override;
  void HideActivePopup() override;
  bool CloseOverflowMenuIfOpen() override;
  bool ShowToolbarActionPopupForAPICall(const std::string& action_id,
                                        ShowPopupCallback callback) override;
  void ToggleExtensionsMenu() override;
  bool HasAnyExtensions() const override;

  // ToolbarActionsModel::Observer:
  void OnToolbarModelInitialized() override;
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarPinnedActionsChanged() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  // TabListInterfaceObserver:
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  void OnTabListDestroyed(TabListInterface& tab_list) override;

  // extensions::PermissionsManager::Observer:
  void OnHostAccessRequestAdded(const extensions::ExtensionId& extension_id,
                                int tab_id) override;
  void OnHostAccessRequestUpdated(const extensions::ExtensionId& extension_id,
                                  int tab_id) override;
  void OnHostAccessRequestRemoved(const extensions::ExtensionId& extension_id,
                                  int tab_id) override;
  void OnHostAccessRequestsCleared(int tab_id) override;
  void OnHostAccessRequestDismissedByUser(const extensions::ExtensionId& id,
                                          const url::Origin& origin) override;
  void OnUserPermissionsSettingsChanged(
      const extensions::PermissionsManager::UserPermissionsSettings& settings)
      override;
  void OnShowAccessRequestsInToolbarChanged(
      const extensions::ExtensionId& extension_id,
      bool can_show_requests) override;

 private:
  // Returns whether any of `actions` given have access to the `web_contents`.
  bool AnyActionHasCurrentSiteAccess(content::WebContents& web_contents) const;

  // Creates and appends an action model to `actions_` vector.
  void AppendActionModel(const ToolbarActionsModel::ActionId& action_id);

  // Returns the current web contents.
  content::WebContents* GetCurrentWebContents() const;

  // The corresponding browser window.
  const raw_ptr<BrowserWindowInterface> browser_;

  // The delegate to retrieve platform-specific information.
  const raw_ptr<Delegate> delegate_;

  const raw_ptr<ToolbarActionsModel> actions_model_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      actions_model_observation_{this};

  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_observation_{this};

  base::ScopedObservation<extensions::PermissionsManager,
                          extensions::PermissionsManager::Observer>
      permissions_manager_observation_{this};

  // The observers that handles platform-specific UI.
  base::ObserverList<Observer> observers_;

  // Actions for all extensions.
  base::flat_map<ToolbarActionsModel::ActionId,
                 std::unique_ptr<ToolbarActionViewModel>>
      actions_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_TOOLBAR_VIEW_MODEL_H_
