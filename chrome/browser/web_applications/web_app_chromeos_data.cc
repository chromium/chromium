// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_chromeos_data.h"

#include <ios>
#include <ostream>
#include <tuple>

namespace web_app {

base::Value WebAppChromeOsData::AsDebugValue() const {
  base::Value::Dict root =
      base::Value::Dict()
          .Set("show_in_launcher", show_in_launcher)
          .Set("show_in_search", show_in_search)
          .Set("show_in_management", show_in_management)
          .Set("is_disabled", is_disabled)
          .Set("oem_installed", oem_installed)
          .Set("handles_file_open_intents", handles_file_open_intents);

  return base::Value(std::move(root));
}

bool operator==(const WebAppChromeOsData& chromeos_data1,
                const WebAppChromeOsData& chromeos_data2) {
  return std::tie(chromeos_data1.show_in_launcher,
                  chromeos_data1.show_in_search,
                  chromeos_data1.show_in_management, chromeos_data1.is_disabled,
                  chromeos_data1.oem_installed,
                  chromeos_data1.handles_file_open_intents) ==
         std::tie(chromeos_data2.show_in_launcher,
                  chromeos_data2.show_in_search,
                  chromeos_data2.show_in_management, chromeos_data2.is_disabled,
                  chromeos_data2.oem_installed,
                  chromeos_data2.handles_file_open_intents);
}

}  // namespace web_app
