// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_chromeos_data.h"

#include <ios>
#include <ostream>
#include <tuple>

namespace web_app {

base::Value WebAppChromeOsData::AsDebugValue() const {
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetBoolKey("show_in_launcher", show_in_launcher);
  root.SetBoolKey("show_in_search", show_in_search);
  root.SetBoolKey("show_in_management", show_in_management);
  root.SetBoolKey("is_disabled", is_disabled);
  root.SetBoolKey("oem_installed", oem_installed);
  root.SetBoolKey("handles_file_open_intents", handles_file_open_intents);
  return root;
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
