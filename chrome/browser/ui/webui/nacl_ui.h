// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NACL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NACL_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The Web UI handler for about:nacl.
class NaClUI : public content::WebUIController {
 public:
  explicit NaClUI(content::WebUI* web_ui);

  NaClUI(const NaClUI&) = delete;
  NaClUI& operator=(const NaClUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NACL_UI_H_
