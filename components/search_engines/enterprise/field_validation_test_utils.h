// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_ENTERPRISE_FIELD_VALIDATION_TEST_UTILS_H_
#define COMPONENTS_SEARCH_ENGINES_ENTERPRISE_FIELD_VALIDATION_TEST_UTILS_H_

#include <optional>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Accepts a dictionary that doesn't have field `field_name` set.
MATCHER_P(FieldNotSet,
          field_name,
          base::StringPrintf("field `%s` is %s",
                             negation ? "set" : "not set",
                             field_name)) {
  return (arg).GetDict().Find(field_name) == nullptr;
}

// Accepts a dictionary that has a string field `field_name` with value
// `expected_value`.
MATCHER_P2(HasStringField,
           field_name,
           expected_value,
           base::StringPrintf("%s string field `%s` with value `%s`",
                              negation ? "does not contain" : "contains",
                              field_name,
                              expected_value.c_str())) {
  const std::string* dict_value = (arg).GetDict().FindString(field_name);
  return dict_value && *dict_value == expected_value;
}

// Accepts a dictionary that has a boolean field `field_name` with value
// `expected_value`.
MATCHER_P2(HasBooleanField,
           field_name,
           expected_value,
           base::StringPrintf("%s boolean field `%s` with value `%d`",
                              negation ? "does not contain" : "contains",
                              field_name,
                              expected_value)) {
  std::optional<bool> dict_value = (arg).GetDict().FindBool(field_name);
  return dict_value && *dict_value == expected_value;
}

// Accepts a dictionary that has a double field `field_name` with non-zero
// value.
MATCHER_P2(HasIntegerField,
           field_name,
           expected_value,
           base::StringPrintf("%s integer field `%s` with value `%d`",
                              negation ? "does not contain" : "contains",
                              field_name,
                              expected_value)) {
  std::optional<int> dict_value = (arg).GetDict().FindInt(field_name);
  return dict_value && *dict_value == expected_value;
}

// Accepts a dictionary that has a double field `field_name` with non-zero
// value.
MATCHER_P(HasDoubleField,
          field_name,
          base::StringPrintf("%s double field `%s` with non-zero value",
                             negation ? "does not contain" : "contains",
                             field_name)) {
  std::optional<double> dict_value = (arg).GetDict().FindDouble(field_name);
  return dict_value && *dict_value != 0.0;
}

}  // namespace policy

#endif  // COMPONENTS_SEARCH_ENGINES_ENTERPRISE_FIELD_VALIDATION_TEST_UTILS_H_
