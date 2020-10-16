// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_JS_EXCEPTION_WEBUI_JS_EXCEPTION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_JS_EXCEPTION_WEBUI_JS_EXCEPTION_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI that controls chrome://webuijsexception.
class WebUIJsExceptionUI : public content::WebUIController {
 public:
  explicit WebUIJsExceptionUI(content::WebUI* web_ui);
  ~WebUIJsExceptionUI() override;
  WebUIJsExceptionUI(const WebUIJsExceptionUI&) = delete;
  WebUIJsExceptionUI& operator=(const WebUIJsExceptionUI&) = delete;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_JS_EXCEPTION_WEBUI_JS_EXCEPTION_UI_H_
