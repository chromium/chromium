// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_system_web_app_data.h"
#include <ios>
#include <ostream>
#include <tuple>

namespace web_app {

bool operator==(const WebAppSystemWebAppData& chromeos_data1,
                const WebAppSystemWebAppData& chromeos_data2) {
  return chromeos_data1.system_app_type == chromeos_data2.system_app_type;
}

std::ostream& operator<<(std::ostream& out,
                         const WebAppSystemWebAppData& chromeos_data) {
  out << "  swa type: " << static_cast<int>(chromeos_data.system_app_type)
      << std::endl;

  return out;
}

}  // namespace web_app
