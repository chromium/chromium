// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_WINDOW_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Image;
}

namespace share {
struct ShareAttempt;
}

namespace sharing_hub {

class ScreenshotCapturedBubble;

#if !BUILDFLAG(IS_CHROMEOS)
class SharingHubBubbleView;
#endif

// Manages the Sharing Hub bubbles for a browser window.
class SharingHubWindowController {
 public:
  DECLARE_USER_DATA(SharingHubWindowController);

  explicit SharingHubWindowController(BrowserWindowInterface* browser);
  SharingHubWindowController(const SharingHubWindowController&) = delete;
  SharingHubWindowController& operator=(const SharingHubWindowController&) =
      delete;
  ~SharingHubWindowController();

  static SharingHubWindowController* From(BrowserWindowInterface* browser);

#if !BUILDFLAG(IS_CHROMEOS)
  // Shows the Sharing Hub bubble.
  SharingHubBubbleView* ShowSharingHubBubble(share::ShareAttempt attempt);
#endif

  // Shows the Screenshot bubble.
  ScreenshotCapturedBubble* ShowScreenshotCapturedBubble(
      content::WebContents* contents,
      const gfx::Image& image);

 private:
  friend class ui::ScopedUnownedUserData<SharingHubWindowController>;

  const raw_ref<BrowserWindowInterface> browser_;
  ui::ScopedUnownedUserData<SharingHubWindowController>
      scoped_unowned_user_data_;
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_WINDOW_CONTROLLER_H_
