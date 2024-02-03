// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_ASH_H_

#include <memory>

#include "chrome/browser/ui/views/frame/browser_view.h"

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

class Browser;

// BrowserViewAsh provides the ClientView for Chrome browser windows on Chrome
// OS under classic ash.
class BrowserViewAsh : public BrowserView {
 public:
  explicit BrowserViewAsh(std::unique_ptr<Browser> browser);

  BrowserViewAsh(const BrowserViewAsh&) = delete;
  BrowserViewAsh& operator=(const BrowserViewAsh&) = delete;

  ~BrowserViewAsh() override = default;

  // views::View:
  void Layout(PassKey) override;

  // views::ClientView:
  void UpdateWindowRoundedCorners(int corner_radius) override;

 private:
  gfx::RoundedCornersF contents_webview_radii_;
  gfx::RoundedCornersF devtools_webview_radii_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_ASH_H_
