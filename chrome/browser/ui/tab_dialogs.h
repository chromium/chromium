// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_DIALOGS_H_
#define CHROME_BROWSER_UI_TAB_DIALOGS_H_

#include <set>

#include "base/functional/callback_forward.h"
#include "base/supports_user_data.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/native_widget_types.h"

class Browser;

namespace content {
class RenderWidgetHost;
class WebContents;
}  // namespace content

namespace ui {
class ProfileSigninConfirmationDelegate;
}

// A cross-platform interface for invoking various tab modal dialogs/bubbles.
class TabDialogs : public base::SupportsUserData::Data {
 public:
  ~TabDialogs() override {}

  // Creates a platform specific instance, and attaches it to |contents|.
  // If an instance is already attached, does nothing.
  static void CreateForWebContents(content::WebContents* contents);

  // Returns the instance that was attached to |contents|.
  // If no instance was attached, returns NULL.
  static TabDialogs* FromWebContents(content::WebContents* contents);

  // Returns the parent view to use when showing a tab modal dialog.
  virtual gfx::NativeView GetDialogParentView() const = 0;

  // Shows the collected cookies dialog box.
  virtual void ShowCollectedCookies() = 0;

  // Shows or hides the hung renderer dialog.
  virtual void ShowHungRendererDialog(
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) = 0;
  virtual void HideHungRendererDialog(
      content::RenderWidgetHost* render_widget_host) = 0;
  virtual bool IsShowingHungRendererDialog() = 0;

  // Shows the deprecated app dialog.
  virtual void ShowDeprecatedAppsDialog(
      const extensions::ExtensionId& optional_launched_extension_id,
      const std::set<extensions::ExtensionId>& deprecated_app_ids,
      content::WebContents* web_contents) = 0;

  // Shows the force installed and deprecated app dialog.
  virtual void ShowForceInstalledDeprecatedAppsDialog(
      const extensions::ExtensionId& app_id,
      content::WebContents* web_contents) = 0;

  // Shows the force installed and deprecated app dialog.
  virtual void ShowForceInstalledPreinstalledDeprecatedAppDialog(
      const extensions::ExtensionId& extension_id,
      content::WebContents* web_contents) = 0;

  // Shows or hides the ManagePasswords bubble.
  // Pass true for |user_action| if this is a user initiated action.
  virtual void ShowManagePasswordsBubble(bool user_action) = 0;
  virtual void HideManagePasswordsBubble() = 0;

 protected:
  static const void* UserDataKey();
};

#endif  // CHROME_BROWSER_UI_TAB_DIALOGS_H_
