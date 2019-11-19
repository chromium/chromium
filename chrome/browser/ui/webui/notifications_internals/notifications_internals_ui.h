// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NOTIFICATIONS_INTERNALS_NOTIFICATIONS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NOTIFICATIONS_INTERNALS_NOTIFICATIONS_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

// The WebUI for chrome://notifications-internals.
class NotificationsInternalsUI : public content::WebUIController {
 public:
  explicit NotificationsInternalsUI(content::WebUI* web_ui);
  ~NotificationsInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationsInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NOTIFICATIONS_INTERNALS_NOTIFICATIONS_INTERNALS_UI_H_
