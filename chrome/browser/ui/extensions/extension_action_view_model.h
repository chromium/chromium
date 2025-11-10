// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_VIEW_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_action_hover_card_types.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "extensions/browser/extension_action_icon_factory.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

class ExtensionActionPlatformDelegate;
class IconWithBadgeImageSource;
enum class PopupShowAction;

namespace extensions {
class Command;
class Extension;
class ExtensionAction;
class ExtensionRegistry;
class SitePermissionsHelper;
}  // namespace extensions

namespace ui {
class ImageModel;
}

// The Views controller for an ExtensionAction that is shown on the toolbar
// (such as a page or browser action).
//
// This class doesn't own the extension or extension action in question. It is
// safe to call methods after the extension is uninstalled, but they will return
// undefined values, except GetId().
//
// TODO(crbug.com/437774758): Enable this class on Desktop Android.
class ExtensionActionViewModel
    : public ToolbarActionViewModel,
      public TabStripModelObserver,
      public ToolbarActionsModel::Observer,
      public extensions::ExtensionActionIconFactory::Observer,
      public extensions::CommandService::Observer,
      public extensions::ExtensionContextMenuModel::PopupDelegate {
 public:
  static std::unique_ptr<ExtensionActionViewModel> Create(
      const extensions::ExtensionId& extension_id,
      BrowserWindowInterface* browser,
      std::unique_ptr<ExtensionActionPlatformDelegate> platform_delegate);

  // Returns whether any of `actions` given have access to the `web_contents`.
  static bool AnyActionHasCurrentSiteAccess(
      const std::vector<std::unique_ptr<ToolbarActionViewModel>>& actions,
      content::WebContents* web_contents);

  ExtensionActionViewModel(const ExtensionActionViewModel&) = delete;
  ExtensionActionViewModel& operator=(const ExtensionActionViewModel&) = delete;

  ~ExtensionActionViewModel() override;

  // ToolbarActionViewModel:
  std::string GetId() const override;
  void SetUpdateObserver(base::RepeatingClosure observer) override;
  ui::ImageModel GetIcon(content::WebContents* web_contents,
                         const gfx::Size& size) override;
  std::u16string GetActionName() const override;
  std::u16string GetActionTitle(
      content::WebContents* web_contents) const override;
  std::u16string GetAccessibleName(
      content::WebContents* web_contents) const override;
  std::u16string GetTooltip(content::WebContents* web_contents) const override;
  ToolbarActionViewModel::HoverCardState GetHoverCardState(
      content::WebContents* web_contents) const override;
  extensions::SitePermissionsHelper::SiteInteraction GetSiteInteraction(
      content::WebContents* web_contents) const override;
  bool IsEnabled(content::WebContents* web_contents) const override;
  bool IsShowingPopup() const override;
  void HidePopup() override;
  gfx::NativeView GetPopupNativeView() override;
  ui::MenuModel* GetContextMenu(
      extensions::ExtensionContextMenuModel::ContextMenuSource
          context_menu_source) override;
  void ExecuteUserAction(InvocationSource source) override;
  void TriggerPopupForAPI(ShowPopupCallback callback) override;
  void RegisterCommand() override;
  void UnregisterCommand() override;

  // TabStripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

  // extensions::CommandService::Observer:
  void OnExtensionCommandAdded(const std::string& extension_id,
                               const std::string& command_name) override;
  void OnExtensionCommandRemoved(const std::string& extension_id,
                                 const std::string& command_name) override;
  void OnCommandServiceDestroying() override;

  // ExtensionContextMenuModel::PopupDelegate:
  void InspectPopup() override;

  // Populates |command| with the command associated with |extension|, if one
  // exists. Returns true if |command| was populated.
  bool GetExtensionCommand(extensions::Command* command) const;

  // Returns true if this controller can handle accelerators (i.e., keyboard
  // commands) on the currently-active WebContents.
  // This must only be called if the extension has an associated command.
  // TODO(devlin): Move accelerator logic out of the platform delegate and into
  // this class.
  bool CanHandleAccelerators() const;

  const extensions::Extension* extension() const { return extension_.get(); }
  BrowserWindowInterface* browser() { return browser_; }
  extensions::ExtensionAction* extension_action() { return extension_action_; }
  const extensions::ExtensionAction* extension_action() const {
    return extension_action_;
  }
  ExtensionActionPlatformDelegate* platform_delegate() {
    return platform_delegate_.get();
  }

  std::unique_ptr<IconWithBadgeImageSource> GetIconImageSourceForTesting(
      content::WebContents* web_contents,
      const gfx::Size& size);

 private:
  // New instances should be instantiated with Create().
  ExtensionActionViewModel(
      scoped_refptr<const extensions::Extension> extension,
      BrowserWindowInterface* browser,
      extensions::ExtensionAction* extension_action,
      extensions::ExtensionRegistry* extension_registry,
      std::unique_ptr<ExtensionActionPlatformDelegate> platform_delegate);

  // Returns the current web contents.
  content::WebContents* GetCurrentWebContents() const;

  // Notifies the observer that the underlying data has been updated.
  void NotifyObserver();

  // extensions::ExtensionActionIconFactory::Observer:
  void OnIconUpdated() override;

  // Checks if the associated |extension| is still valid by checking its
  // status in the registry. Since the OnExtensionUnloaded() notifications are
  // not in a deterministic order, it's possible that the view tries to refresh
  // itself before we're notified to remove it.
  bool ExtensionIsValid() const;

  // Begins the process of showing the popup for the extension action on the
  // current web contents. |by_user| is true if popup is being triggered by a
  // user action.
  // The popup may not be shown synchronously if the extension is hidden and
  // first needs to slide itself out.
  void TriggerPopup(PopupShowAction show_action,
                    bool by_user,
                    ShowPopupCallback callback);

  // Returns the image source for the icon.
  std::unique_ptr<IconWithBadgeImageSource> GetIconImageSource(
      content::WebContents* web_contents,
      const gfx::Size& size);

  // The extension ID.
  extensions::ExtensionId extension_id_;

  // The extension associated with the action we're displaying.
  scoped_refptr<const extensions::Extension> extension_;

  // The corresponding browser window.
  const raw_ptr<BrowserWindowInterface> browser_;

  // The corresponding profile.
  const raw_ptr<Profile> profile_;

  // The browser action this view represents. The ExtensionAction is not owned
  // by this class.
  const raw_ptr<extensions::ExtensionAction, DanglingUntriaged>
      extension_action_;

  // The context menu model for the extension.
  std::unique_ptr<extensions::ExtensionContextMenuModel> context_menu_model_;

  // Our observer.
  base::RepeatingClosure observer_;

  // The delegate to handle platform-specific implementations.
  std::unique_ptr<ExtensionActionPlatformDelegate> platform_delegate_;

  // The object that will be used to get the browser action icon for us.
  // It may load the icon asynchronously (in which case the initial icon
  // returned by the factory will be transparent), so we have to observe it for
  // updates to the icon.
  extensions::ExtensionActionIconFactory icon_factory_;

  // The associated ExtensionRegistry; cached for quick checking.
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;

  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      toolbar_model_observation_{this};

  base::ScopedObservation<extensions::CommandService,
                          extensions::CommandService::Observer>
      command_service_observation_{this};
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_VIEW_MODEL_H_
