// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"

class GURL;

namespace tab_groups {
class TabGroupId;
}

namespace chrome {

class BrowserTabStripModelDelegate : public TabStripModelDelegate {
 public:
  explicit BrowserTabStripModelDelegate(Browser* browser);

  BrowserTabStripModelDelegate(const BrowserTabStripModelDelegate&) = delete;
  BrowserTabStripModelDelegate& operator=(const BrowserTabStripModelDelegate&) =
      delete;

  ~BrowserTabStripModelDelegate() override;

 private:
  // Overridden from TabStripModelDelegate:
  void AddTabAt(const GURL& url,
                int index,
                bool foreground,
                std::optional<tab_groups::TabGroupId> group) override;
  Browser* CreateNewStripWithTabs(std::vector<NewStripContents> tabs,
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
  std::optional<SessionID> CreateHistoricalTab(
      content::WebContents* contents) override;
  void CreateHistoricalGroup(const tab_groups::TabGroupId& group) override;
  void GroupAdded(const tab_groups::TabGroupId& group) override;
  void WillCloseGroup(const tab_groups::TabGroupId& group) override;
  void GroupCloseStopped(const tab_groups::TabGroupId& group) override;
  bool RunUnloadListenerBeforeClosing(content::WebContents* contents) override;
  bool ShouldRunUnloadListenerBeforeClosing(
      content::WebContents* contents) override;
  bool ShouldDisplayFavicon(content::WebContents* contents) const override;
  bool CanReload() const override;
  void AddToReadLater(content::WebContents* web_contents) override;
  bool SupportsReadLater() override;
  bool IsForWebApp() override;
  void CopyURL(content::WebContents* web_contents) override;
  void GoBack(content::WebContents* web_contents) override;
  bool CanGoBack(content::WebContents* web_contents) override;
  bool IsNormalWindow() override;
  BrowserWindowInterface* GetBrowserWindowInterface() override;
  void OnGroupsDestruction(const std::vector<tab_groups::TabGroupId>& group_ids,
                           base::OnceCallback<void()> close_callback,
                           bool is_bulk_operation) override;
  void OnRemovingAllTabsFromGroups(
      const std::vector<tab_groups::TabGroupId>& group_ids,
      base::OnceCallback<void()> callback) override;

  void CloseFrame();

  // Returns whether the browser has the right conditions for creating
  // historical tabs or groups.
  bool BrowserSupportsHistoricalEntries();

  const raw_ptr<Browser> browser_;

  // The following factory is used to close the frame at a later time.
  base::WeakPtrFactory<BrowserTabStripModelDelegate> weak_factory_{this};
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_
