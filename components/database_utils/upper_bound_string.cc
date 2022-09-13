// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/database_utils/upper_bound_string.h"

#include <limits>

namespace database_utils {

std::string UpperBoundString(const std::string& prefix) {
  std::string upper_bound = prefix;
  upper_bound.push_back(std::numeric_limits<unsigned char>::max());
  return upper_bound;
}

}  // namespace database_utils
