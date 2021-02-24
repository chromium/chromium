// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_NAVIGATION_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_NAVIGATION_BUTTON_CONTAINER_H_

#include "chrome/browser/command_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace {
class WebAppToolbarBackButton;
class WebAppToolbarReloadButton;
}  // namespace

class BackForwardButton;
class ReloadButton;
class Browser;
class BrowserView;

// Holds controls in the far left of the toolbar.
class WebAppNavigationButtonContainer : public views::View,
                                        public CommandObserver {
 public:
  METADATA_HEADER(WebAppNavigationButtonContainer);
  explicit WebAppNavigationButtonContainer(BrowserView* browser_view);
  ~WebAppNavigationButtonContainer() override;

  BackForwardButton* back_button();
  ReloadButton* reload_button();

  void SetIconColor(SkColor icon_color);

 protected:
  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override;

 private:
  // The containing browser.
  Browser* const browser_;

  // These members are owned by the views hierarchy.
  WebAppToolbarBackButton* back_button_ = nullptr;
  WebAppToolbarReloadButton* reload_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_NAVIGATION_BUTTON_CONTAINER_H_
