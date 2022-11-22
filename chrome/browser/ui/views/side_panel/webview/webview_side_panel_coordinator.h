// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_WEBVIEW_WEBVIEW_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_WEBVIEW_WEBVIEW_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/textfield/textfield_controller.h"

class Browser;
class SidePanelRegistry;

namespace views {
class Textfield;
class View;
class WebView;
}  // namespace views

class WebViewSidePanelCoordinator
    : public BrowserUserData<WebViewSidePanelCoordinator>,
      public content::WebContentsObserver,
      public views::TextfieldController {
 public:
  explicit WebViewSidePanelCoordinator(Browser* browser);
  WebViewSidePanelCoordinator(const WebViewSidePanelCoordinator&) = delete;
  WebViewSidePanelCoordinator& operator=(const WebViewSidePanelCoordinator&) =
      delete;
  ~WebViewSidePanelCoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* registry);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 private:
  friend class BrowserUserData<WebViewSidePanelCoordinator>;

  std::unique_ptr<views::View> CreateView();

  raw_ptr<views::Textfield> location_;
  raw_ptr<views::WebView> webview_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_WEBVIEW_WEBVIEW_SIDE_PANEL_COORDINATOR_H_
