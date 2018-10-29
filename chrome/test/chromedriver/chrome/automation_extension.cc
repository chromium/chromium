// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/automation_extension.h"

#include <utility>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

AutomationExtension::AutomationExtension(std::unique_ptr<WebView> web_view)
    : web_view_(std::move(web_view)) {}

AutomationExtension::~AutomationExtension() {}

Status AutomationExtension::CaptureScreenshot(std::string* screenshot) {
  base::ListValue args;
  std::unique_ptr<base::Value> result;
  Status status = web_view_->CallAsyncFunction(
      std::string(),
      "captureScreenshot",
      args,
      base::TimeDelta::FromSeconds(10),
      &result);
  if (status.IsError())
    return Status(status.code(), "cannot take screenshot", status);
  if (!result->GetAsString(screenshot))
    return Status(kUnknownError, "screenshot is not a string");
  return Status(kOk);
}

Status AutomationExtension::LaunchApp(const std::string& id) {
  base::ListValue args;
  args.AppendString(id);
  std::unique_ptr<base::Value> result;
  return web_view_->CallAsyncFunction(std::string(),
                                      "launchApp",
                                      args,
                                      base::TimeDelta::FromSeconds(10),
                                      &result);
}
