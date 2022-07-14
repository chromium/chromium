// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_TEST_UTIL_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_TEST_UTIL_H_

#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

namespace {

const std::string kTestUuidBrowser = "040b6112-67f2-4d3c-8ba8-53a117272eba";
const std::string kBrowserUrl = "https://example.com/";
const std::string kTestUuidChromeAndProgressive =
    "7f4b7ff0-970a-41bb-aa91-f6c3e2724207";
const std::string kBrowserTemplateName = "BrowserTest";
const std::string kChromePwaTemplateName = "ChromeAppTest";

}  // namespace

namespace desks_storage::desk_test_util {

constexpr char kTestChromeAppId[] = "test_chrome_app_id";
constexpr char kTestPwaAppId[] = "test_pwa_app_id";
constexpr char kTestSwaAppId[] = "test_swa_app_id";
constexpr char kTestArcAppId[] = "test_arc_app_id";
constexpr char kTestLacrosChromeAppId[] = "test_lacros_chrome_app_id";
constexpr char kTestUnsupportedAppId[] = "test_unsupported_app_id";
constexpr char kTestChromeAppId1[] = "test_chrome_app_1";
constexpr char kTestPwaAppId1[] = "test_pwa_app_1";

const std::string kValidPolicyTemplateBrowser =
    "{\"version\":1,\"uuid\":\"" + kTestUuidBrowser + "\",\"name\":\"" +
    kBrowserTemplateName +
    "\",\"created_time_usec\":\"1633535632\",\"updated_time_usec\": "
    "\"1633535632\",\"desk_type\":\"TEMPLATE\",\"desk\":{\"apps\":[{\"window_"
    "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
    "\"url\":\"" +
    kBrowserUrl +
    "\"},{\"url\":\"https://"
    "example.com/"
    "2\"}],\"tab_groups\":[{\"first_"
    "index\":1,\"last_index\":2,\"title\":\"sample_tab_"
    "group\",\"color\":\"GREY\",\"is_collapsed\":false}],\"active_tab_index\":"
    "1,\"first_non_pinned_tab_index\":1,\"window_id\":0,"
    "\"display_id\":\"100\",\"event_flag\":0}]}}";

const std::string kValidPolicyTemplateBrowserMinimized =
    "{\"version\":1,\"uuid\":\"" + kTestUuidBrowser + "\",\"name\":\"" +
    kBrowserTemplateName +
    "\",\"created_time_usec\":\"1633535632\",\"updated_time_usec\": "
    "\"1633535632\",\"desk_type\":\"TEMPLATE\",\"desk\":{\"apps\":[{\"window_"
    "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
    "state\":\"MINIMIZED\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
    "\"url\":\"" +
    kBrowserUrl +
    "\"},{\"url\":\"https://"
    "example.com/"
    "2\"}],\"tab_groups\":[{\"first_"
    "index\":1,\"last_index\":2,\"title\":\"sample_tab_"
    "group\",\"color\":\"GREY\",\"is_collapsed\":false}],\"active_tab_index\":"
    "1,\"first_non_pinned_tab_index\":1,\"window_id\":0,"
    "\"display_id\":\"100\",\"event_flag\":0,\"pre_minimized_window_state\":"
    "\"NORMAL\"}]}}";

const std::string kValidPolicyTemplateChromeAndProgressive =
    "{\"version\":1,\"uuid\":\"" + kTestUuidChromeAndProgressive +
    "\",\"name\":\"" + kChromePwaTemplateName +
    "\",\"created_time_usec\":\"1633535632000\",\"updated_time_usec\": "
    "\"1633535632\",\"desk_type\":\"SAVE_AND_RECALL\",\"desk\":{\"apps\":[{"
    "\"window_"
    "bound\":{"
    "\"left\":200,\"top\":200,\"height\":1000,\"width\":1000},\"window_state\":"
    "\"PRIMARY_SNAPPED\",\"z_index\":2,\"app_type\":\"CHROME_APP\",\"app_id\":"
    "\"" +
    desk_test_util::kTestChromeAppId1 +
    "\",\"window_id\":0,\"display_id\":\"100\",\"event_flag\":0, "
    "\"snap_percent\":75},{\"window_"
    "bound\":{\"left\":0,\"top\":0,\"height\":120,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"CHROME_APP\",\"app_id\":"
    "\"" +
    desk_test_util::kTestPwaAppId1 +
    "\",\"window_id\":1,\"display_id\":"
    "\"100\",\"event_flag\":0}]}}";

const std::string kPolicyTemplateWithoutType =
    "{\"version\":1,\"uuid\":\"" + kTestUuidBrowser + "\",\"name\":\"" +
    kBrowserTemplateName +
    "\",\"created_time_usec\":\"1633535632\",\"updated_time_usec\": "
    "\"1633535632\",\"desk\":{\"apps\":[{\"window_"
    "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
    "\"url\":\"" +
    kBrowserUrl + "\"},{\"url\":\"" + kBrowserUrl +
    "\"}],\"tab_groups\":[{\"first_"
    "index\":1,\"last_index\":2,\"title\":\"sample_tab_"
    "group\",\"color\":\"GREY\",\"is_collapsed\":false}],\"active_tab_index\":"
    "1,\"first_non_pinned_tab_index\":1,\"window_id\":0,"
    "\"display_id\":\"100\",\"event_flag\":0}]}}";

// Populates the given cache with test app information.
void PopulateAppRegistryCache(AccountId account_id,
                              apps::AppRegistryCache* cache);

void AddAppIdToAppRegistryCache(AccountId account_id,
                                apps::AppRegistryCache* cache,
                                const char* app_id);

}  // namespace desks_storage::desk_test_util

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_TEST_UTIL_H_
