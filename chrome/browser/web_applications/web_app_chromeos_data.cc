// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_chromeos_data.h"

#include <ios>
#include <ostream>
#include <tuple>
#include "base/json/values_util.h"

namespace web_app {

WebAppChromeOsData::WebAppChromeOsData() = default;
WebAppChromeOsData::WebAppChromeOsData(const WebAppChromeOsData&) = default;
WebAppChromeOsData::~WebAppChromeOsData() = default;

base::Value WebAppChromeOsData::AsDebugValue() const {
  auto root = base::Value::Dict()
                  .Set("show_in_launcher", show_in_launcher)
                  .Set("show_in_search", show_in_search)
                  .Set("show_in_management", show_in_management)
                  .Set("is_disabled", is_disabled)
                  .Set("oem_installed", oem_installed)
                  .Set("handles_file_open_intents", handles_file_open_intents);
  if (app_profile_path.has_value()) {
    root.Set("app_profile_path",
             base::FilePathToValue(app_profile_path.value()));
  } else {
    root.Set("app_profile_path", "");
  }
  return base::Value(std::move(root));
}

bool operator==(const WebAppChromeOsData& chromeos_data1,
                const WebAppChromeOsData& chromeos_data2) {
  auto AsTuple = [](const WebAppChromeOsData& data) {
    return std::tie(data.show_in_launcher, data.show_in_search,
                    data.show_in_management, data.is_disabled,
                    data.oem_installed, data.handles_file_open_intents,
                    data.app_profile_path);
  };
  return AsTuple(chromeos_data1) == AsTuple(chromeos_data2);
}

}  // namespace web_app
