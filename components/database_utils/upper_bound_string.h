// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATABASE_UTILS_UPPER_BOUND_STRING_H_
#define COMPONENTS_DATABASE_UTILS_UPPER_BOUND_STRING_H_

#include <string>

namespace database_utils {

// Returns a string with the following property:
// Condition (str >= prefix AND str < UpperBoundString(prefix))
// is satisfied for all strs that start with prefix and only for them.
// This can be used for optimization purposes to avoid using the LIKE operator.
// prefix must be a UTF-8 string.
std::string UpperBoundString(const std::string& prefix);

}  // namespace database_utils

#endif  // COMPONENTS_DATABASE_UTILS_UPPER_BOUND_STRING_H_
