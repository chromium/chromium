// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_DIALOGS_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_DIALOGS_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/ui/tab_dialogs.h"

// Views implementation of TabDialogs interface.
class TabDialogsViews : public TabDialogs {
 public:
  explicit TabDialogsViews(content::WebContents* contents);
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
  void ShowProfileSigninConfirmation(
      Browser* browser,
      Profile* profile,
      const std::string& username,
      std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate) override;
  void ShowManagePasswordsBubble(bool user_action) override;
  void HideManagePasswordsBubble() override;

 private:
  content::WebContents* web_contents_;  // Weak. Owns this.

  DISALLOW_COPY_AND_ASSIGN(TabDialogsViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_DIALOGS_VIEWS_H_
