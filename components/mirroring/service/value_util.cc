// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/value_util.h"

namespace mirroring {

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

}  // namespace mirroring
