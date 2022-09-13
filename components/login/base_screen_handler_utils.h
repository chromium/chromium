// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOGIN_BASE_SCREEN_HANDLER_UTILS_H_
#define COMPONENTS_LOGIN_BASE_SCREEN_HANDLER_UTILS_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "components/login/login_export.h"

namespace login {

using StringList = std::vector<std::string>;

StringList LOGIN_EXPORT ConvertToStringList(const base::Value::List& list);

}  // namespace login

#endif  // COMPONENTS_LOGIN_BASE_SCREEN_HANDLER_UTILS_H_
