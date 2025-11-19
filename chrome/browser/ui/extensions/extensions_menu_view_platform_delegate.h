// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_PLATFORM_DELEGATE_H_

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "extensions/common/extension_id.h"

namespace content {
class WebContents;
}  // namespace content

class ExtensionsMenuViewModel;

class ExtensionsMenuViewPlatformDelegate {
 public:
  virtual ~ExtensionsMenuViewPlatformDelegate() = default;

  // Attaches the delegate to a platform-agnostic menu view model. It is called
  // by the model on its constructor.
  virtual void AttachToModel(ExtensionsMenuViewModel* model) = 0;

  // Detaches the delegate from a platform-agnostic menu view model. It is
  // called by the model on its destructor.
  virtual void DetachFromModel() = 0;

  // Notifies the delegate that the active web contents changed to
  // `web_contents`.
  virtual void OnActiveWebContentsChanged(
      content::WebContents* web_contents) = 0;

  // Notifies the delegate that a new host access request was added or updated
  // for `extension_id` on `web_contents`.
  virtual void OnHostAccessRequestAddedOrUpdated(
      const extensions::ExtensionId& extension_id,
      content::WebContents* web_contents) = 0;

  // Notifies the delegate that the host access request for
  // `extension_id` was removed.
  virtual void OnHostAccessRequestRemoved(
      const extensions::ExtensionId& extension_id) = 0;

  // Notifies the delegate that host access requests on the current site were
  // cleared.
  virtual void OnHostAccessRequestsCleared() = 0;

  // Notifies the delegate that the host access requests for `extension_id` on
  // the current site was dismissed.
  virtual void OnHostAccessRequestDismissedByUser(
      const extensions::ExtensionId& extension_id) = 0;

  virtual void OnShowHostAccessRequestsInToolbarChanged(
      const extensions::ExtensionId& extension_id,
      bool can_show_requests) = 0;

  // Notifies the delegate that a new toolbar action was added.
  virtual void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) = 0;

  // Notifies the delegate that toolbar action with `action_id` was removed.
  virtual void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) = 0;

  // Notifies the delegate that a toolbar action was updated.
  virtual void OnToolbarActionUpdated() = 0;

  // Notifies the delegate that the toolbar actions model was initialized
  virtual void OnToolbarModelInitialized() = 0;

  // Notifies the delegate that the pinned toolbar actions have changed
  virtual void OnToolbarPinnedActionsChanged() = 0;

  // Notifies the delegate that the user permissions settings changed on the
  // current site.
  // TODO(crbug.com/449814184): Rename to `OnUserPermissionsSettingsChanged`
  // after we finish migrating all PermissionsManager::Observer method from the
  // platform delegate to the model, since same name causes parameter type
  // mismatch.
  virtual void OnPermissionsSettingsChanged() = 0;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_PLATFORM_DELEGATE_H_
