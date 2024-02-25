// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_SAVED_DESK_TEST_UTIL_H_
#define COMPONENTS_DESKS_STORAGE_CORE_SAVED_DESK_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/app_restore/restore_data.h"
#include "url/gurl.h"

namespace desks_storage::saved_desk_test_util {

// Adds a Chrome browser window to `out_restore_data`.
void AddBrowserWindow(bool is_lacros,
                      int window_id,
                      std::vector<GURL> urls,
                      app_restore::RestoreData* out_restore_data);

// Adds a PWA window to `out_restore_data`.
void AddPwaWindow(bool is_lacros,
                  int window_id,
                  std::string url,
                  app_restore::RestoreData* out_restore_data);

// Adds a Generic app window to `out_restore_data`.
void AddGenericAppWindow(int window_id,
                         std::string app_id,
                         app_restore::RestoreData* out_restore_data);

}  // namespace desks_storage::saved_desk_test_util

#endif  // COMPONENTS_DESKS_STORAGE_CORE_SAVED_DESK_TEST_UTIL_H_
