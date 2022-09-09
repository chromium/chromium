// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TEST_TAB_STRIP_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_TEST_TAB_STRIP_MODEL_DELEGATE_H_

#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}  // namespace content

// Mock TabStripModelDelegate.
class TestTabStripModelDelegate : public TabStripModelDelegate {
 public:
  TestTabStripModelDelegate();
  TestTabStripModelDelegate(const TestTabStripModelDelegate&) = delete;
  TestTabStripModelDelegate& operator=(const TestTabStripModelDelegate&) =
      delete;
  ~TestTabStripModelDelegate() override;

  // Overridden from TabStripModelDelegate:
  void AddTabAt(const GURL& url,
                int index,
                bool foregroud,
                absl::optional<tab_groups::TabGroupId> group) override;
  Browser* CreateNewStripWithContents(std::vector<NewStripContents> contentses,
                                      const gfx::Rect& window_bounds,
                                      bool maximize) override;
  void WillAddWebContents(content::WebContents* contents) override;
  int GetDragActions() const override;
  bool CanDuplicateContentsAt(int index) override;
  bool IsTabStripEditable() override;
  void DuplicateContentsAt(int index) override;
  void MoveToExistingWindow(const std::vector<int>& indices,
                            int browser_index) override;
  bool CanMoveTabsToWindow(const std::vector<int>& indices) override;
  void MoveTabsToNewWindow(const std::vector<int>& indices) override;
  void MoveGroupToNewWindow(const tab_groups::TabGroupId& group) override;
  absl::optional<SessionID> CreateHistoricalTab(
      content::WebContents* contents) override;
  void CreateHistoricalGroup(const tab_groups::TabGroupId& group) override;
  void GroupCloseStopped(const tab_groups::TabGroupId& group) override;
  bool ShouldRunUnloadListenerBeforeClosing(
      content::WebContents* contents) override;
  bool RunUnloadListenerBeforeClosing(content::WebContents* contents) override;
  bool ShouldDisplayFavicon(content::WebContents* web_contents) const override;
  bool CanReload() const override;
  void AddToReadLater(content::WebContents* web_contents) override;
  bool SupportsReadLater() override;
  void CacheWebContents(
      const std::vector<std::unique_ptr<TabStripModel::DetachedWebContents>>&
          web_contents) override;
  void FollowSite(content::WebContents* web_contents) override;
  void UnfollowSite(content::WebContents* web_contents) override;
  bool IsForWebApp() override;
  void CopyURL(content::WebContents* web_contents) override;
};

#endif  // CHROME_BROWSER_UI_TABS_TEST_TAB_STRIP_MODEL_DELEGATE_H_
