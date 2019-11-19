// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/developer_console_logger.h"

#include "content/public/browser/web_contents.h"

namespace payments {

DeveloperConsoleLogger::DeveloperConsoleLogger(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

DeveloperConsoleLogger::~DeveloperConsoleLogger() {}

void DeveloperConsoleLogger::Warn(const std::string& warning_message) const {
  if (!enabled_)
    return;
  if (web_contents() && web_contents()->GetMainFrame()) {
    web_contents()->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning, warning_message);
  } else {
    ErrorLogger::Warn(warning_message);
  }
}

void DeveloperConsoleLogger::Error(const std::string& error_message) const {
  if (!enabled_)
    return;
  if (web_contents() && web_contents()->GetMainFrame()) {
    web_contents()->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, error_message);
  } else {
    ErrorLogger::Error(error_message);
  }
}

}  // namespace payments
