// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/value_util.h"

namespace mirroring {

bool GetInt(const base::Value& value, const std::string& key, int32_t* result) {
  auto* found = value.GetDict().Find(key);
  if (!found || found->is_none())
    return true;
  if (found->is_int()) {
    *result = found->GetInt();
    return true;
  }
  return false;
}

bool GetDouble(const base::Value& value,
               const std::string& key,
               double* result) {
  auto* found = value.GetDict().Find(key);
  if (!found || found->is_none())
    return true;
  if (found->is_double()) {
    *result = found->GetDouble();
    return true;
  }
  if (found->is_int()) {
    *result = found->GetInt();
    return true;
  }
  return false;
}

bool GetString(const base::Value& value,
               const std::string& key,
               std::string* result) {
  auto* found = value.GetDict().Find(key);
  if (!found || found->is_none())
    return true;
  if (found->is_string()) {
    *result = found->GetString();
    return true;
  }
  return false;
}

bool GetBool(const base::Value& value, const std::string& key, bool* result) {
  auto* found = value.GetDict().Find(key);
  if (!found || found->is_none())
    return true;
  if (found->is_bool()) {
    *result = found->GetBool();
    return true;
  }
  return false;
}

bool GetIntArray(const base::Value& value,
                 const std::string& key,
                 std::vector<int32_t>* result) {
  auto* found = value.GetDict().Find(key);
  if (!found || found->is_none())
    return true;
  if (!found->is_list())
    return false;
  for (const auto& number_value : found->GetList()) {
    if (number_value.is_int())
      result->emplace_back(number_value.GetInt());
    else
      return false;
  }
  return true;
}

bool GetStringArray(const base::Value& value,
                    const std::string& key,
                    std::vector<std::string>* result) {
  auto* found = value.GetDict().Find(key);
  if (!found || found->is_none())
    return true;
  if (!found->is_list())
    return false;
  for (const auto& string_value : found->GetList()) {
    if (string_value.is_string())
      result->emplace_back(string_value.GetString());
    else
      return false;
  }
  return true;
}

}  // namespace mirroring
