// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_CUSTOM_CURSOR_SUPPRESSOR_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_CUSTOM_CURSOR_SUPPRESSOR_H_

#include <map>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class WebContents;
}  // namespace content

// While active, it suppresses custom cursors exceeding a given size limit on
// all the active `WebContents` or all `Browser`s of the current profile.
class CustomCursorSuppressor : public BrowserListObserver,
                               public TabStripModelObserver {
 public:
  CustomCursorSuppressor();
  CustomCursorSuppressor(const CustomCursorSuppressor&) = delete;
  CustomCursorSuppressor(CustomCursorSuppressor&&) = delete;
  CustomCursorSuppressor& operator=(const CustomCursorSuppressor&) = delete;
  CustomCursorSuppressor& operator=(CustomCursorSuppressor&&) = delete;
  ~CustomCursorSuppressor() override;

  // Starts suppressing cursors with height or width >= `max_dimension_dips` on
  // all active tabs of all browser windows.
  void Start(int max_dimension_dips = 0);
  // Stops suppressing custom cursors.
  void Stop();
  bool IsSuppressing() const;

  // Returns the ids of `RenderFrameHost`s on which custom cursors are
  // suppressed. Note that not every id needs to correspond to an active
  // `RenderFrameHost` - some may already have been deleted.
  std::vector<content::GlobalRenderFrameHostId>
  SuppressedRenderFrameHostIdsForTesting() const;

 private:
  // Disallows custom cursors beyond the permitted size on `web_contents`. If
  // `this` is already disallowing custom cursors on `web_contents`, this is a
  // no-op.
  void SuppressForWebContents(content::WebContents& web_contents);

  // BrowserListObserver:
  // Starts observing the tab strip model of `browser`. Note that there is
  // no corresponding `OnBrowserRemoved`, since `TabStripModelObserver`
  // already handles model destruction itself.
  void OnBrowserAdded(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};

  int max_dimension_dips_ = 0;

  std::map<content::GlobalRenderFrameHostId, base::ScopedClosureRunner>
      disallow_custom_cursor_scopes_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_CUSTOM_CURSOR_SUPPRESSOR_H_
