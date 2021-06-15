// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UI_H_

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

// The Web UI controller for the chrome://whats-new page.
class WhatsNewUI : public content::WebUIController {
 public:
  explicit WhatsNewUI(content::WebUI* web_ui);
  ~WhatsNewUI() override;

  WhatsNewUI(const WhatsNewUI&) = delete;
  WhatsNewUI& operator=(const WhatsNewUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UI_H_
