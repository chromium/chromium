// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/schema_org/common/time.h"

#include <sstream>

namespace schema_org {

std::optional<base::TimeDelta> ParseISO8601Duration(const std::string& str) {
  if (str.empty() || str[0] != 'P')
    return std::nullopt;

  base::TimeDelta duration;

  std::string time = "";
  int time_index = str.find("T");
  if (time_index == -1)
    return std::nullopt;

  time = str.substr(time_index + 1);
  std::stringstream t(time);
  char unit;
  int amount;

  while (t >> amount) {
    t >> unit;
    switch (unit) {
      case 'H':
        duration = duration + base::Hours(amount);
        break;
      case 'M':
        duration = duration + base::Minutes(amount);
        break;
      case 'S':
        duration = duration + base::Seconds(amount);
        break;
      default:
        return std::nullopt;
    }
  }

  return duration;
}

}  // namespace schema_org
