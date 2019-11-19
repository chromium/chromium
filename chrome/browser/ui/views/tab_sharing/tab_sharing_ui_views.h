// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_UI_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_UI_VIEWS_H_

#include <map>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}
namespace infobars {
class InfoBar;
}

class Profile;

class TabSharingUIViews : public TabSharingUI,
                          public BrowserListObserver,
                          public TabStripModelObserver,
                          public infobars::InfoBarManager::Observer,
                          public content::WebContentsObserver {
 public:
  TabSharingUIViews(const content::DesktopMediaID& media_id,
                    base::string16 app_name);
  ~TabSharingUIViews() override;

  // MediaStreamUI:
  // Called when tab sharing has started. Creates infobars on all tabs.
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback) override;

  // TabSharingUI:
  // Runs |source_callback_| to start sharing the tab containing |infobar|.
  // Removes infobars on all tabs; OnStarted() will recreate the infobars with
  // updated title and buttons.
  void StartSharing(infobars::InfoBar* infobar) override;

  // Runs |stop_callback_| to stop sharing |shared_tab_|. Removes infobars on
  // all tabs.
  void StopSharing() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void CreateInfobarsForAllTabs();
  void CreateInfobarForWebContents(content::WebContents* contents);
  void RemoveInfobarsForAllTabs();

  void CreateTabCaptureIndicator();

  std::map<content::WebContents*, infobars::InfoBar*> infobars_;
  content::DesktopMediaID shared_tab_media_id_;
  const base::string16 app_name_;
  content::WebContents* shared_tab_;
  base::string16 shared_tab_name_;
  Profile* profile_;
  std::unique_ptr<content::MediaStreamUI> tab_capture_indicator_ui_;

  content::MediaStreamUI::SourceCallback source_callback_;
  base::OnceClosure stop_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_UI_VIEWS_H_
