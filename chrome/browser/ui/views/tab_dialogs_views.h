// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_DIALOGS_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_DIALOGS_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tab_dialogs.h"

// Views implementation of TabDialogs interface.
class TabDialogsViews : public TabDialogs {
 public:
  explicit TabDialogsViews(content::WebContents* contents);

  TabDialogsViews(const TabDialogsViews&) = delete;
  TabDialogsViews& operator=(const TabDialogsViews&) = delete;

  ~TabDialogsViews() override;

  // TabDialogs:
  gfx::NativeView GetDialogParentView() const override;
  void ShowCollectedCookies() override;
  void ShowHungRendererDialog(
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override;
  void HideHungRendererDialog(
      content::RenderWidgetHost* render_widget_host) override;
  bool IsShowingHungRendererDialog() override;
  void ShowManagePasswordsBubble(bool user_action) override;
  void HideManagePasswordsBubble() override;
  void ShowDeprecatedAppsDialog(
      const extensions::ExtensionId& optional_launched_extension_id,
      const std::set<extensions::ExtensionId>& deprecated_app_ids,
      content::WebContents* web_contents) override;
  void ShowForceInstalledDeprecatedAppsDialog(
      const extensions::ExtensionId& app_id,
      content::WebContents* web_contents) override;
  void ShowForceInstalledPreinstalledDeprecatedAppDialog(
      const extensions::ExtensionId& app_id,
      content::WebContents* web_contents) override;

 private:
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owns this.
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_DIALOGS_VIEWS_H_
