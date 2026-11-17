// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READ_LATER_SIDE_PANEL_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READ_LATER_SIDE_PANEL_WEB_VIEW_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"

class BrowserWindowInterface;
class Profile;

namespace tabs {
class TabInterface;
}  // namespace tabs

class ReadLaterSidePanelWebView : public SidePanelWebUIViewT<ReadingListUI> {
  using SidePanelWebUIViewT_ReadingListUI = SidePanelWebUIViewT<ReadingListUI>;
  METADATA_HEADER(ReadLaterSidePanelWebView, SidePanelWebUIViewT_ReadingListUI)

 public:
  ReadLaterSidePanelWebView(Profile* profile,
                            SidePanelEntryScope& scope,
                            base::RepeatingClosure close_cb);
  ReadLaterSidePanelWebView(const ReadLaterSidePanelWebView&) = delete;
  ReadLaterSidePanelWebView& operator=(const ReadLaterSidePanelWebView&) =
      delete;
  ~ReadLaterSidePanelWebView() override;

  void UpdateActiveURL(content::WebContents* contents);
  void UpdateActiveURLToActiveTab();

 private:
  void OnActiveTabChanged(BrowserWindowInterface* browser);
  void UpdateActiveTabSubscription(tabs::TabInterface* tab);
  void OnTabUIChanged();

  base::CallbackListSubscription active_tab_subscription_;
  base::CallbackListSubscription tab_ui_subscription_;
  base::WeakPtrFactory<ReadLaterSidePanelWebView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READING_LIST_READ_LATER_SIDE_PANEL_WEB_VIEW_H_
