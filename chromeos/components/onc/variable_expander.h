// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ONC_VARIABLE_EXPANDER_H_
#define CHROMEOS_COMPONENTS_ONC_VARIABLE_EXPANDER_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"

namespace base {
class Value;
}

namespace chromeos {

// Expands variables in a string or base::Value. Allows setting which variables
// to expand and the corresponding values. Also supports substrings with given
// zero-based position or position/count, e.g. if the variable name is
// "machine_name" and the value is "chromebook", then the this class expands
//   "${machine_name}"     to "chromebook",
//   "${machine_name,6}"   to "book" (position 6 of "chromebook" to end) and
//   "${machine_name,2,4}" to "rome" (4 characters from position 2).
// Sample usage:
//   std::string str = "I run ${machine_name,0,6} on my ${machine_name}";
//   VariableExpander expander({{"machine_name", "chromebook"}});
//   expander.ExpandString(&str);
//   // str is now "I run chrome on my chromebook"
class COMPONENT_EXPORT(CHROMEOS_ONC) VariableExpander {
 public:
  // Takes a map of variables to values.
  explicit VariableExpander(base::flat_map<std::string, std::string> variables);

  VariableExpander(const VariableExpander&) = delete;
  VariableExpander& operator=(const VariableExpander&) = delete;

  ~VariableExpander();

  // Expands all variables in |str|. Returns true if no error has occurred.
  // Returns false if at least one variable was malformed and could not be
  // expanded (the good ones are still expanded).
  bool ExpandString(std::string* str) const;

  // Calls ExpandString on every string contained in |value|. Recursively
  // handles all hierarchy levels. Returns true if no error has occurred.
  // Returns false if at least one variable was malformed and could not be
  // expanded (the good ones are still expanded).
  bool ExpandValue(base::Value* value) const;

 private:
  // Maps variable -> value.
  const base::flat_map<std::string, std::string> variables_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ONC_VARIABLE_EXPANDER_H_
