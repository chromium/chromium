// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_TEST_UTIL_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_TEST_UTIL_H_

#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

namespace desks_storage {

namespace desk_test_util {

constexpr char kTestChromeAppId[] = "test_chrome_app_id";
constexpr char kTestPwaAppId[] = "test_pwa_app_id";
constexpr char kTestSwaAppId[] = "test_swa_app_id";
constexpr char kTestArcAppId[] = "test_arc_app_id";
constexpr char kTestLacrosChromeAppId[] = "test_lacros_chrome_app_id";
constexpr char kTestUnsupportedAppId[] = "test_unsupported_app_id";
constexpr char kTestChromeAppId1[] = "test_chrome_app_1";
constexpr char kTestPwaAppId1[] = "test_pwa_app_1";

// Populates the given cache with test app information.
void PopulateAppRegistryCache(AccountId account_id,
                              apps::AppRegistryCache* cache);

void AddAppIdToAppRegistryCache(AccountId account_id,
                                apps::AppRegistryCache* cache,
                                const char* app_id);

}  // namespace desk_test_util

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_TEST_UTIL_H_
