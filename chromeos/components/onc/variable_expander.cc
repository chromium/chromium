// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/onc/variable_expander.h"

#include <algorithm>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/values.h"

namespace {

// Expects ",n" or ",n,m" in |range|. Puts n into |start| and m into |count| if
// present. Returns true if |range| was well formatted and parsing the numbers
// succeeded.
bool ParseRange(std::string_view range, size_t* start, size_t* count) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      range, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK(!parts.empty());
  if (!parts[0].empty())
    return false;
  if (parts.size() > 1 && !base::StringToSizeT(parts[1], start))
    return false;
  if (parts.size() > 2 && !base::StringToSizeT(parts[2], count))
    return false;
  if (parts.size() > 3)
    return false;
  return true;
}

// Expands all occurrences of a variable with name |variable_name| to
// |replacement| or parts of it. This method handles the following variants:
//   - ${variable_name}           -> |replacement|
//   - ${variable_name,pos}       -> |replacement.substr(pos)|
//   - ${variable_name,pos,count} -> |replacement.substr(pos,count)|
// Strictly enforces the format (up to whitespace), e.g.
// ${variable_name ,  2 , 9  } works, but ${variable_name,2o,9e} doesn't.
// Returns true if no error occurred.
bool Expand(std::string_view variable_name,
            std::string_view replacement,
            std::string* str) {
  std::string token = base::StrCat({"${", variable_name});
  size_t token_start = 0;
  bool no_error = true;
  int count = 0;
  while ((token_start = str->find(token, token_start)) != std::string::npos) {
    // Exit if |token| is found too often.
    if (++count > 100) {
      LOG(ERROR) << "Maximum replacement count exceeded for " << variable_name
                 << " in string '" << *str << "'";
      no_error = false;
      break;
    }

    // Find the closing braces.
    const size_t range_start = token_start + token.size();
    const size_t range_end = str->find("}", range_start);
    if (range_end == std::string::npos) {
      LOG(ERROR) << "Closing braces not found for " << variable_name
                 << " in string '" << *str << "'";
      ++token_start;
      no_error = false;
      continue;
    }

    // Full variable, e.g. ${machine_name} or ${machine_name,8,3}.
    DCHECK_GE(range_end, range_start);
    const std::string_view full_token =
        std::string_view(*str).substr(token_start, range_end + 1 - token_start);

    // Determine if the variable defines a range, e.g. ${machine_name,8,3}.
    size_t replacement_start = 0;
    size_t replacement_count = std::string::npos;
    if (range_end > range_start) {
      const std::string_view range =
          std::string_view(*str).substr(range_start, range_end - range_start);
      if (!ParseRange(range, &replacement_start, &replacement_count)) {
        LOG(ERROR) << "Invalid range definition for " << variable_name
                   << " in string '" << *str << "'";
        token_start += full_token.size();
        no_error = false;
        continue;
      }
    }

    const std::string_view replacement_part = replacement.substr(
        std::min(replacement_start, replacement.size()), replacement_count);
    // Don't use ReplaceSubstringsAfterOffset here, it can lead to a doubling
    // of tokens, see VariableExpanderTest.DoesNotRecurse test.
    base::ReplaceFirstSubstringAfterOffset(str, token_start, full_token,
                                           replacement_part);
    token_start += replacement_part.size();
  }
  return no_error;
}

}  // namespace

namespace chromeos {

VariableExpander::VariableExpander(
    base::flat_map<std::string, std::string> variables)
    : variables_(std::move(variables)) {}

VariableExpander::~VariableExpander() = default;

bool VariableExpander::ExpandString(std::string* str) const {
  bool no_error = true;
  for (const auto& kv : variables_)
    no_error &= Expand(kv.first, kv.second, str);
  return no_error;
}

bool VariableExpander::ExpandValue(base::Value* value) const {
  bool no_error = true;
  switch (value->type()) {
    case base::Value::Type::STRING: {
      std::string expanded_str = value->GetString();
      no_error &= ExpandString(&expanded_str);
      *value = base::Value(expanded_str);
      break;
    }

    case base::Value::Type::DICT: {
      for (const auto child : value->GetDict()) {
        no_error &= ExpandValue(&child.second);
      }
      break;
    }

    case base::Value::Type::LIST: {
      for (base::Value& child : value->GetList())
        no_error &= ExpandValue(&child);
      break;
    }

    case base::Value::Type::BOOLEAN:
    case base::Value::Type::INTEGER:
    case base::Value::Type::DOUBLE:
    case base::Value::Type::BINARY:
    case base::Value::Type::NONE: {
      // Nothing to do here.
      break;
    }
  }
  return no_error;
}

}  // namespace chromeos
