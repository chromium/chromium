// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_NOTIFICATION_TESTER_NOTIFICATION_TESTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_NOTIFICATION_TESTER_NOTIFICATION_TESTER_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

// The UI controller for NotificationTester page.
class NotificationTesterUI : public content::WebUIController {
 public:
  explicit NotificationTesterUI(content::WebUI* web_ui);
  NotificationTesterUI(const NotificationTesterUI&) = delete;
  NotificationTesterUI& operator=(const NotificationTesterUI&) = delete;
  ~NotificationTesterUI() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_NOTIFICATION_TESTER_NOTIFICATION_TESTER_UI_H_
