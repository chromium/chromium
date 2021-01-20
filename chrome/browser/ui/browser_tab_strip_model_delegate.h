// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
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
  ~BrowserTabStripModelDelegate() override;

 private:
  // Overridden from TabStripModelDelegate:
  void AddTabAt(const GURL& url,
                int index,
                bool foreground,
                base::Optional<tab_groups::TabGroupId> group) override;
  Browser* CreateNewStripWithContents(std::vector<NewStripContents> contentses,
                                      const gfx::Rect& window_bounds,
                                      bool maximize) override;
  void WillAddWebContents(content::WebContents* contents) override;
  int GetDragActions() const override;
  bool CanDuplicateContentsAt(int index) override;
  void DuplicateContentsAt(int index) override;
  void MoveToExistingWindow(const std::vector<int>& indices,
                            int browser_index) override;
  std::vector<base::string16> GetExistingWindowsForMoveMenu() const override;
  bool CanMoveTabsToWindow(const std::vector<int>& indices) override;
  void MoveTabsToNewWindow(const std::vector<int>& indices) override;
  void MoveGroupToNewWindow(const tab_groups::TabGroupId& group) override;
  base::Optional<SessionID> CreateHistoricalTab(
      content::WebContents* contents) override;
  bool RunUnloadListenerBeforeClosing(content::WebContents* contents) override;
  bool ShouldRunUnloadListenerBeforeClosing(
      content::WebContents* contents) override;
  bool ShouldDisplayFavicon(content::WebContents* contents) const override;

  void CloseFrame();

  Browser* const browser_;

  // The following factory is used to close the frame at a later time.
  base::WeakPtrFactory<BrowserTabStripModelDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserTabStripModelDelegate);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_TAB_STRIP_MODEL_DELEGATE_H_
