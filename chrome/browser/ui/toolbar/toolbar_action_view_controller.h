// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_CONTROLLER_H_

#include <string>

#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "chrome/browser/ui/toolbar/toolbar_action_hover_card_types.h"
#include "ui/base/models/image_model.h"

namespace content {
class WebContents;
}

namespace gfx {
class Size;
}

namespace ui {
class MenuModel;
}

class ToolbarActionViewDelegate;
class ToolbarActionView;

// The basic controller class for an action that is shown on the toolbar -
// an extension action (like browser actions) or a component action (like
// Media Router).
class ToolbarActionViewController {
 public:
  // The source for the action invocation. Used in UMA; do not reorder or delete
  // entries.
  enum class InvocationSource {
    // The action was invoked from a command (keyboard shortcut).
    kCommand = 0,

    // The action was invoked by the user activating (via mouse or keyboard)
    // the action button in the toolbar.
    kToolbarButton = 1,

    // The action was invoked by the user activating (via mouse or keyboard)
    // the entry in the Extensions Menu.
    kMenuEntry = 2,

    // The action was invoked by the user activiating (via mouse or keyboard)
    // the entry in the legacy overflow (3-dot) menu.
    // Removed 2021/04.
    // kLegacyOverflowedEntry = 3,

    // The action was invoked programmatically via an API.
    kApi = 4,

    // The action was invoked by the user activating (via mouse or keyboard) the
    // request access button in the toolbar
    kRequestAccessButton = 5,

    kMaxValue = kRequestAccessButton,
  };

  // State for the toolbar action view's hover card.
  struct HoverCardState {
    enum class SiteAccess {
      // All extensions are allowed on the current site by the user.
      kAllExtensionsAllowed,

      // All extensions are blocked on the current site by the user.
      kAllExtensionsBlocked,

      // The extension has access to the current site.
      kExtensionHasAccess,

      // The extension requests access to the current site.
      kExtensionRequestsAccess,

      // The extension does not want access to the current site.
      kExtensionDoesNotWantAccess,
    };

    enum class AdminPolicy {
      kNone,
      // Extension is force pinned by administrator.
      kPinnedByAdmin,

      // Extension if force installed by administrator.
      kInstalledByAdmin,
    };

    SiteAccess site_access;
    AdminPolicy policy;
  };

  virtual ~ToolbarActionViewController() = default;

  // Returns the unique ID of this particular action. For extensions, this is
  // the extension id; for component actions, this is the name of the component.
  virtual std::string GetId() const = 0;

  // Sets the view delegate, which can handle most of the front-end logic.
  virtual void SetDelegate(ToolbarActionViewDelegate* delegate) = 0;

  // Returns the icon to use for the given |web_contents| and |size|.
  virtual ui::ImageModel GetIcon(content::WebContents* web_contents,
                                 const gfx::Size& size) = 0;

  // Returns the name of the action.
  virtual std::u16string GetActionName() const = 0;

  // Returns the title of the action on the given `web_contents`, which may be
  // different than the action's name.
  virtual std::u16string GetActionTitle(
      content::WebContents* web_contents) const = 0;

  // Returns the accessible name to use for the given |web_contents|.
  // May be passed null, or a |web_contents| that returns -1 for
  // |sessions::SessionTabHelper::IdForTab(..)|.
  virtual std::u16string GetAccessibleName(
      content::WebContents* web_contents) const = 0;

  // Returns the tooltip to use for the given |web_contents|.
  virtual std::u16string GetTooltip(
      content::WebContents* web_contents) const = 0;

  // Returns the hover card state to use for the given `web_contents`.
  virtual HoverCardState GetHoverCardState(
      content::WebContents* web_contents) const = 0;

  // Returns true if the action should be enabled on the given |web_contents|.
  virtual bool IsEnabled(content::WebContents* web_contents) const = 0;

  // Returns whether there is currently a popup visible.
  virtual bool IsShowingPopup() const = 0;

  // Hides the current popup, if one is visible.
  virtual void HidePopup() = 0;

  // Returns the native view for the popup, if one is active.
  virtual gfx::NativeView GetPopupNativeView() = 0;

  // Returns the context menu model, or null if no context menu should be shown.
  virtual ui::MenuModel* GetContextMenu(
      extensions::ExtensionContextMenuModel::ContextMenuSource
          context_menu_source) = 0;

  // Called when a context menu is shown from `source` so the controller can
  // perform any necessary setup.
  virtual void OnContextMenuShown(
      extensions::ExtensionContextMenuModel::ContextMenuSource source) {}

  // Called when a context menu has closed from `source` so the controller can
  // perform any necessary cleanup.
  virtual void OnContextMenuClosed(
      extensions::ExtensionContextMenuModel::ContextMenuSource source) {}

  // Executes the default behavior associated with the action. This should only
  // be called as a result of a user action.
  virtual void ExecuteUserAction(InvocationSource source) = 0;

  // Shows the toolbar action popup as a result of an API call. It is the
  // caller's responsibility to guarantee it is valid to show a popup (i.e.,
  // the action is enabled, has a popup, etc).
  virtual void TriggerPopupForAPI(ShowPopupCallback callback) = 0;

  // Updates the current state of the action.
  virtual void UpdateState() = 0;

  // Updates the hover card for `action_view` based on `update_type`.
  virtual void UpdateHoverCard(ToolbarActionView* action_view,
                               ToolbarActionHoverCardUpdateType update_type) {}

  // Registers an accelerator. Called when the view is added to a widget.
  virtual void RegisterCommand() {}

  // Unregisters an accelerator. Called when the view is removed from a widget.
  virtual void UnregisterCommand() {}

  // Returns the PageInteractionStatus for the current page.
  virtual extensions::SitePermissionsHelper::SiteInteraction GetSiteInteraction(
      content::WebContents* web_contents) const = 0;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_CONTROLLER_H_
