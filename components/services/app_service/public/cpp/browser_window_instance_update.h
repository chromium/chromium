// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_BROWSER_WINDOW_INSTANCE_UPDATE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_BROWSER_WINDOW_INSTANCE_UPDATE_H_

#include "base/unguessable_token.h"

namespace apps {

struct BrowserWindowInstanceUpdate {
  base::UnguessableToken id;
  std::string window_id;
  bool is_active = false;
  uint32_t browser_session_id = 0;
  uint32_t restored_browser_session_id = 0;
  bool is_incognito = false;
  uint64_t lacros_profile_id = 0;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_BROWSER_WINDOW_INSTANCE_UPDATE_H_
