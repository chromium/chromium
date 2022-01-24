// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_system_web_app_data.h"
#include <ios>
#include <ostream>
#include <tuple>

namespace web_app {

base::Value WebAppSystemWebAppData::AsDebugValue() const {
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetIntKey("system_app_type", static_cast<int>(system_app_type));
  return root;
}

bool operator==(const WebAppSystemWebAppData& chromeos_data1,
                const WebAppSystemWebAppData& chromeos_data2) {
  return chromeos_data1.system_app_type == chromeos_data2.system_app_type;
}

}  // namespace web_app
