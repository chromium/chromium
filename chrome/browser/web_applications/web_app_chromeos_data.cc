// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_chromeos_data.h"

namespace web_app {

base::Value WebAppChromeOsData::AsDebugValue() const {
  auto root = base::Value::Dict()
                  .Set("show_in_launcher", show_in_launcher)
                  .Set("show_in_search_and_shelf", show_in_search_and_shelf)
                  .Set("show_in_management", show_in_management)
                  .Set("is_disabled", is_disabled)
                  .Set("oem_installed", oem_installed)
                  .Set("handles_file_open_intents", handles_file_open_intents);
  return base::Value(std::move(root));
}

}  // namespace web_app
