// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_HANDLER_H_

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/tab_strip/tab_before_unload_tracker.h"
#include "chrome/browser/ui/webui/tab_strip/thumbnail_tracker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"

class Browser;
class TabStripUIEmbedder;

class TabStripUIHandler : public content::WebUIMessageHandler,
                          public TabStripModelObserver {
 public:
  explicit TabStripUIHandler(Browser* browser, TabStripUIEmbedder* embedder);
  ~TabStripUIHandler() override;

  void NotifyLayoutChanged();
  void NotifyReceivedKeyboardFocus();

  // TabStripModelObserver:
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void TabGroupedStateChanged(base::Optional<tab_groups::TabGroupId> group,
                              content::WebContents* contents,
                              int index) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index) override;
  void TabBlockedStateChanged(content::WebContents* contents,
                              int index) override;

 protected:
  // content::WebUIMessageHandler:
  void OnJavascriptAllowed() override;
  void RegisterMessages() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TabStripUIHandlerTest, CloseTab);
  FRIEND_TEST_ALL_PREFIXES(TabStripUIHandlerTest, GetGroupVisualData);
  FRIEND_TEST_ALL_PREFIXES(TabStripUIHandlerTest, GroupTab);
  FRIEND_TEST_ALL_PREFIXES(TabStripUIHandlerTest, MoveGroup);
  FRIEND_TEST_ALL_PREFIXES(TabStripUIHandlerTest, MoveGroupAcrossProfiles);
  FRIEND_TEST_ALL_PREFIXES(TabStripUIHandlerTest, MoveGroupAcrossWindows);
  FRIEND_TEST_ALL_PREFIXES(TabStripUIHandlerTest, MoveTab);
  FRIEND_TEST_ALL_PREFIXES(TabStripUIHandlerTest, MoveTabAcrossProfiles);
  FRIEND_TEST_ALL_PREFIXES(TabStripUIHandlerTest, MoveTabAcrossWindows);
  FRIEND_TEST_ALL_PREFIXES(TabStripUIHandlerTest,
                           RemoveTabIfInvalidContextMenu);
  FRIEND_TEST_ALL_PREFIXES(TabStripUIHandlerTest, UngroupTab);

  void HandleCreateNewTab(const base::ListValue* args);
  base::DictionaryValue GetTabData(content::WebContents* contents, int index);
  base::DictionaryValue GetTabGroupData(TabGroup* group);
  void HandleGetTabs(const base::ListValue* args);
  void HandleGetGroupVisualData(const base::ListValue* args);
  void HandleGetThemeColors(const base::ListValue* args);
  void HandleCloseContainer(const base::ListValue* args);
  void HandleCloseTab(const base::ListValue* args);
  void HandleShowBackgroundContextMenu(const base::ListValue* args);
  void HandleShowEditDialogForGroup(const base::ListValue* args);
  void HandleShowTabContextMenu(const base::ListValue* args);
  void HandleGetLayout(const base::ListValue* args);
  void HandleGroupTab(const base::ListValue* args);
  void HandleUngroupTab(const base::ListValue* args);
  void HandleMoveGroup(const base::ListValue* args);
  void HandleMoveTab(const base::ListValue* args);
  void HandleSetThumbnailTracked(const base::ListValue* args);
  void HandleReportTabActivationDuration(const base::ListValue* args);
  void HandleReportTabDataReceivedDuration(const base::ListValue* args);
  void HandleReportTabCreationDuration(const base::ListValue* args);
  void HandleThumbnailUpdate(content::WebContents* tab,
                             ThumbnailTracker::CompressedThumbnailData image);
  void OnTabCloseCancelled(content::WebContents* tab);
  void ReportTabDurationHistogram(const char* histogram_fragment,
                                  int tab_count,
                                  base::TimeDelta duration);

  Browser* const browser_;
  TabStripUIEmbedder* const embedder_;
  ThumbnailTracker thumbnail_tracker_;
  tab_strip_ui::TabBeforeUnloadTracker tab_before_unload_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TabStripUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_HANDLER_H_
