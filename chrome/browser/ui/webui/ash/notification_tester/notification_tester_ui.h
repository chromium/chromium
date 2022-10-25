// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_NOTIFICATION_TESTER_NOTIFICATION_TESTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_NOTIFICATION_TESTER_NOTIFICATION_TESTER_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace ash {

class NotificationTesterUI;

class NotificationTesterUIConfig
    : public content::DefaultWebUIConfig<NotificationTesterUI> {
 public:
  NotificationTesterUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUINotificationTesterHost) {}
};

// The UI controller for NotificationTester page.
class NotificationTesterUI : public content::WebUIController {
 public:
  explicit NotificationTesterUI(content::WebUI* web_ui);
  NotificationTesterUI(const NotificationTesterUI&) = delete;
  NotificationTesterUI& operator=(const NotificationTesterUI&) = delete;
  ~NotificationTesterUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_NOTIFICATION_TESTER_NOTIFICATION_TESTER_UI_H_
