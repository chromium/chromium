// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTED_APP_SIZE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTED_APP_SIZE_H_

#include <cstdint>

namespace web_app {

struct ComputedAppSize {
  uint64_t app_size_in_bytes = 0;
  uint64_t data_size_in_bytes = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTED_APP_SIZE_H_
