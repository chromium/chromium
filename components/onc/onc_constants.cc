// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/onc/onc_constants.h"

#include <string>
#include <string_view>

#include "base/strings/strcat.h"

namespace onc::network_config {

std::string CellularProperty(std::string_view property) {
  return base::StrCat({kCellular, ".", property});
}

std::string TetherProperty(std::string_view property) {
  return base::StrCat({kTether, ".", property});
}

std::string VpnProperty(std::string_view property) {
  return base::StrCat({kVPN, ".", property});
}

std::string WifiProperty(std::string_view property) {
  return base::StrCat({kWiFi, ".", property});
}

}  // namespace onc::network_config
