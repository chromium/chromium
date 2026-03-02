// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAD_TAB_CONTROLLER_H_
#define CHROME_BROWSER_UI_SAD_TAB_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/sad_tab.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/view_tracker.h"

namespace content {
class WebContents;
}

class SadTabView;

class SadTabController : public SadTab {
 public:
  SadTabController(content::WebContents* web_contents, SadTabKind kind);
  SadTabController(const SadTabController&) = delete;
  SadTabController& operator=(const SadTabController&) = delete;
  ~SadTabController() override;

  // SadTab:
  void ReinstallInWebView() override;

  void SetBackgroundRadii(const gfx::RoundedCornersF& radii);
  gfx::RoundedCornersF GetBackgroundRadii() const;
  void RequestFocus();

 private:
  views::ViewTracker view_tracker_;
  views::ViewTracker contents_view_tracker_;
  std::unique_ptr<SadTabView> owned_sad_tab_view_;
  base::WeakPtrFactory<SadTabController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SAD_TAB_CONTROLLER_H_
