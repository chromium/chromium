// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_chromeos_data.h"

#include <ios>
#include <ostream>
#include <tuple>

namespace web_app {

bool operator==(const WebAppChromeOsData& chromeos_data1,
                const WebAppChromeOsData& chromeos_data2) {
  return std::tie(chromeos_data1.show_in_launcher,
                  chromeos_data1.show_in_search,
                  chromeos_data1.show_in_management, chromeos_data1.is_disabled,
                  chromeos_data1.oem_installed) ==
         std::tie(chromeos_data2.show_in_launcher,
                  chromeos_data2.show_in_search,
                  chromeos_data2.show_in_management, chromeos_data2.is_disabled,
                  chromeos_data2.oem_installed);
}

std::ostream& operator<<(std::ostream& out,
                         const WebAppChromeOsData& chromeos_data) {
  out << "  show_in_launcher: " << chromeos_data.show_in_launcher << std::endl;
  out << "  show_in_search: " << chromeos_data.show_in_search << std::endl;
  out << "  show_in_management: " << chromeos_data.show_in_management
      << std::endl;
  out << "  is_disabled: " << chromeos_data.is_disabled << std::endl;
  out << "  oem_installed: " << chromeos_data.oem_installed << std::endl;

  return out;
}

}  // namespace web_app
