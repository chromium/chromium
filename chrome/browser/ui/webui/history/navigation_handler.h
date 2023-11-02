// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_NAVIGATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_NAVIGATION_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace webui {

// A place to add handlers for messages shared across all WebUI pages.
class NavigationHandler : public content::WebUIMessageHandler {
 public:
  NavigationHandler();

  NavigationHandler(const NavigationHandler&) = delete;
  NavigationHandler& operator=(const NavigationHandler&) = delete;

  ~NavigationHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void HandleNavigateToUrl(const base::Value::List& args);
};

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_NAVIGATION_HANDLER_H_
