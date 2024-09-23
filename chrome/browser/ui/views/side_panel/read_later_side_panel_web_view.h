// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_LATER_SIDE_PANEL_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_LATER_SIDE_PANEL_WEB_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"

class Browser;

class ReadLaterSidePanelWebView : public SidePanelWebUIViewT<ReadingListUI>,
                                  public TabStripModelObserver {
  using SidePanelWebUIViewT_ReadingListUI = SidePanelWebUIViewT<ReadingListUI>;
  METADATA_HEADER(ReadLaterSidePanelWebView, SidePanelWebUIViewT_ReadingListUI)

 public:
  ReadLaterSidePanelWebView(Browser* browser, base::RepeatingClosure close_cb);
  ReadLaterSidePanelWebView(const ReadLaterSidePanelWebView&) = delete;
  ReadLaterSidePanelWebView& operator=(const ReadLaterSidePanelWebView&) =
      delete;
  ~ReadLaterSidePanelWebView() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  void UpdateActiveURL(content::WebContents* contents);
  void UpdateActiveURLToActiveTab();

 private:
  const raw_ptr<Browser> browser_;
  base::WeakPtrFactory<ReadLaterSidePanelWebView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_LATER_SIDE_PANEL_WEB_VIEW_H_
