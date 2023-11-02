// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_DEVELOPER_CONSOLE_LOGGER_H_
#define COMPONENTS_PAYMENTS_CONTENT_DEVELOPER_CONSOLE_LOGGER_H_

#include "base/memory/weak_ptr.h"
#include "components/payments/core/error_logger.h"

namespace content {
class WebContents;
}

namespace payments {

// Logs messages for web developers to the developer console.
class DeveloperConsoleLogger : public ErrorLogger {
 public:
  explicit DeveloperConsoleLogger(content::WebContents* web_contents);

  DeveloperConsoleLogger(const DeveloperConsoleLogger&) = delete;
  DeveloperConsoleLogger& operator=(const DeveloperConsoleLogger&) = delete;

  ~DeveloperConsoleLogger() override;

  // Gets the WebContents being logged to.
  content::WebContents* web_contents() { return web_contents_.get(); }

  // ErrorLogger;
  void Warn(const std::string& warning_message) const override;
  void Error(const std::string& error_message) const override;

 private:
  base::WeakPtr<content::WebContents> web_contents_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_DEVELOPER_CONSOLE_LOGGER_H_
