// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_NAVIGATION_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_NAVIGATION_BUTTON_CONTAINER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/command_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class BackForwardButton;
class ReloadButton;
class Browser;
class BrowserView;
class ToolbarButtonProvider;

// Holds controls in the far left of the toolbar.
class WebAppNavigationButtonContainer : public views::View,
                                        public CommandObserver {
  METADATA_HEADER(WebAppNavigationButtonContainer, views::View)

 public:
  WebAppNavigationButtonContainer(
      BrowserView* browser_view,
      ToolbarButtonProvider* toolbar_button_provider);
  ~WebAppNavigationButtonContainer() override;

  BackForwardButton* back_button();
  ReloadButton* reload_button();

 protected:
  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override;

 private:
  // The containing browser.
  const raw_ptr<Browser> browser_;

  // These members are owned by the views hierarchy.
  raw_ptr<BackForwardButton> back_button_ = nullptr;
  raw_ptr<ReloadButton> reload_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_NAVIGATION_BUTTON_CONTAINER_H_
