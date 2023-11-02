// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/login/base_screen_handler_utils.h"

#include <string>
#include <vector>

#include "base/values.h"

namespace login {

StringList ConvertToStringList(const base::Value::List& list) {
  StringList result;
  for (const auto& val : list)
    result.push_back(val.GetString());
  return result;
}

}  // namespace login
