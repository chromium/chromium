// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/command_metrics_test_helper.h"

#include "base/strings/strcat.h"

namespace web_app::test {

std::vector<std::string> GetInstallCommandResultHistogramNames(
    std::string_view command,
    std::string_view type) {
  return {
      "WebApp.InstallCommand.ResultCode",
      base::StrCat({"WebApp.InstallCommand", command, ".ResultCode"}),
      base::StrCat({"WebApp.InstallCommand", type, ".ResultCode"}),
      base::StrCat({"WebApp.InstallCommand", command, type, ".ResultCode"}),
  };
}

std::vector<std::string> GetInstallCommandSourceHistogramNames(
    std::string_view command,
    std::string_view type) {
  return {
      "WebApp.InstallCommand.Surface",
      base::StrCat({"WebApp.InstallCommand", command, ".Surface"}),
      base::StrCat({"WebApp.InstallCommand", type, ".Surface"}),
      base::StrCat({"WebApp.InstallCommand", command, type, ".Surface"}),
  };
}
}  // namespace web_app::test
