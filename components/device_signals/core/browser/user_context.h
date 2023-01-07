// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_CONTEXT_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_CONTEXT_H_

#include <string>

namespace device_signals {

struct UserContext {
  // GAIA ID of the user.
  std::string user_id;
};

bool operator==(const UserContext& l, const UserContext& r);

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_CONTEXT_H_
