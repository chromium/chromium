// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAD_TAB_CONTROLLER_H_
#define CHROME_BROWSER_UI_SAD_TAB_CONTROLLER_H_

#include <memory>
#include <vector>

#include "chrome/browser/ui/sad_tab.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace content {
class WebContents;
}

class SadTabView;

class SadTabController : public SadTab {
 public:
  SadTabController(content::WebContents* sad_tab_web_contents,
                   SadTabKind sad_tab_kind);
  SadTabController(const SadTabController&) = delete;
  SadTabController& operator=(const SadTabController&) = delete;
  ~SadTabController() override;

  // SadTab:
  void ReinstallInWebView() override;

  void SetBackgroundRadii(const gfx::RoundedCornersF& radii);
  gfx::RoundedCornersF GetBackgroundRadii() const;
  void RequestFocus();

 private:
  std::unique_ptr<SadTabView> view_;
};

#endif  // CHROME_BROWSER_UI_SAD_TAB_CONTROLLER_H_
