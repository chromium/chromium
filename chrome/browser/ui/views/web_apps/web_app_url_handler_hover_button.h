// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_URL_HANDLER_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_URL_HANDLER_HOVER_BUTTON_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/web_apps/web_app_hover_button.h"
#include "chrome/browser/web_applications/url_handler_launch_params.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/view_factory.h"

class GURL;

namespace web_app {
class WebAppProvider;
}

// WebAppUrlHandlerHoverButton is a hoverable button with a primary left-hand
// icon, a title and a subtitle.
class WebAppUrlHandlerHoverButton : public WebAppHoverButton {
 public:
  METADATA_HEADER(WebAppUrlHandlerHoverButton);
  // Creates a hoverable button with the given elements for an app, like so:
  //
  // +-------------------------------------------------------------------+
  // |      |    title                                                   |
  // | icon |                                                            |
  // |      |    subtitle                                                |
  // +-------------------------------------------------------------------+
  //
  WebAppUrlHandlerHoverButton(
      views::Button::PressedCallback callback,
      const web_app::UrlHandlerLaunchParams& url_handler_launch_params,
      web_app::WebAppProvider* provider,
      const std::u16string& display_name,
      const GURL& app_start_url);

  // Creates a hoverable button for the browser option, like so:
  //
  // +-------------------------------------------------------------------+
  // |      |                                                            |
  // | icon |    title                                                   |
  // |      |                                                            |
  // +-------------------------------------------------------------------+
  //
  explicit WebAppUrlHandlerHoverButton(views::Button::PressedCallback callback);
  WebAppUrlHandlerHoverButton(const WebAppUrlHandlerHoverButton&) = delete;
  WebAppUrlHandlerHoverButton& operator=(const WebAppUrlHandlerHoverButton&) =
      delete;
  ~WebAppUrlHandlerHoverButton() override;

  void MarkAsSelected(const ui::Event* event) override;
  void MarkAsUnselected(const ui::Event* event) override;

  const web_app::UrlHandlerLaunchParams& url_handler_launch_params() const {
    return url_handler_launch_params_;
  }

  bool is_app() const { return is_app_; }
  bool selected() { return selected_; }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  const web_app::UrlHandlerLaunchParams url_handler_launch_params_;
  // True if the current WebAppUrlHandlerHoverButton is for an app, false if
  // it's for the browser.
  const bool is_app_;
  bool selected_ = false;
};

BEGIN_VIEW_BUILDER(, WebAppUrlHandlerHoverButton, WebAppHoverButton)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, WebAppUrlHandlerHoverButton)

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_URL_HANDLER_HOVER_BUTTON_H_
