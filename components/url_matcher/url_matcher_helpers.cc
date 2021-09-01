// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/url_matcher_helpers.h"

#include <stddef.h>

#include "base/values.h"

namespace url_matcher {
namespace url_matcher_helpers {

// Converts a ValueList |value| of strings into a vector. Returns true if
// successful.
bool GetAsStringVector(const base::Value* value,
                       std::vector<std::string>* out) {
  if (!value->is_list())
    return false;

  for (const base::Value& item : value->GetList()) {
    if (!item.is_string())
      return false;

    out->push_back(item.GetString());
  }
  return true;
}

}  // namespace url_matcher_helpers
}  // namespace url_matcher
