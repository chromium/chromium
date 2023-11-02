// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/developer_console_logger.h"

#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace payments {

DeveloperConsoleLogger::DeveloperConsoleLogger(
    content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()) {}

DeveloperConsoleLogger::~DeveloperConsoleLogger() = default;

void DeveloperConsoleLogger::Warn(const std::string& warning_message) const {
  if (!enabled_)
    return;
  if (web_contents_ && web_contents_->GetPrimaryMainFrame()) {
    web_contents_->GetPrimaryMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning, warning_message);
  } else {
    ErrorLogger::Warn(warning_message);
  }
}

void DeveloperConsoleLogger::Error(const std::string& error_message) const {
  if (!enabled_)
    return;
  if (web_contents_ && web_contents_->GetPrimaryMainFrame()) {
    web_contents_->GetPrimaryMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, error_message);
  } else {
    ErrorLogger::Error(error_message);
  }
}

}  // namespace payments
