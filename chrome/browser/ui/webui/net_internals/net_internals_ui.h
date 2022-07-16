// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

class NetInternalsUI : public content::WebUIController {
 public:
  explicit NetInternalsUI(content::WebUI* web_ui);

  NetInternalsUI(const NetInternalsUI&) = delete;
  NetInternalsUI& operator=(const NetInternalsUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_H_
