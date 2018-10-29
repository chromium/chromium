// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_AUTOMATION_EXTENSION_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_AUTOMATION_EXTENSION_H_

#include <memory>
#include <string>

#include "base/macros.h"

namespace base {
class DictionaryValue;
}

class Status;
class WebView;

// Automates Chrome through the extension APIs.
class AutomationExtension {
 public:
  explicit AutomationExtension(std::unique_ptr<WebView> web_view);
  ~AutomationExtension();

  // Captures the visible part of the current tab as a base64-encoded PNG.
  // Returns |kForbidden| for security restricted pages.
  Status CaptureScreenshot(std::string* screenshot);

  // Launches an app with the specified id.
  Status LaunchApp(const std::string& id);

 private:
  std::unique_ptr<WebView> web_view_;

  DISALLOW_COPY_AND_ASSIGN(AutomationExtension);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_AUTOMATION_EXTENSION_H_
