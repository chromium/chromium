// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_TEST_UTIL_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_TEST_UTIL_H_

#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

namespace desks_storage::desk_test_util {

inline constexpr char kTestChromeAppId[] = "test_chrome_app_id";
inline constexpr char kTestPwaAppId[] = "test_pwa_app_id";
inline constexpr char kTestSwaAppId[] = "test_swa_app_id";
inline constexpr char kTestArcAppId[] = "test_arc_app_id";
inline constexpr char kTestLacrosChromeAppId[] = "test_lacros_chrome_app_id";
inline constexpr char kTestUnsupportedAppId[] = "test_unsupported_app_id";
inline constexpr char kTestChromeAppId1[] = "test_chrome_app_1";
inline constexpr char kTestPwaAppId1[] = "test_pwa_app_1";

inline constexpr char kValidPolicyTemplateBrowser[] =
    "{\"auto_launch_on_startup\": "
    "false,\"version\":1,\"uuid\":\"040b6112-67f2-4d3c-8ba8-53a117272eba\","
    "\"name\":"
    "\"BrowserTest\",\"created_time_usec\":\"1633535632\",\"updated_time_"
    "usec\": "
    "\"1633535632\",\"desk_type\":\"TEMPLATE\",\"desk\":{\"apps\":[{\"window_"
    "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"app_id\":"
    "\"mgndgikekgjfcpckkfioiadnlibdjbkf\",\"tabs\":[{"
    "\"url\":\"https://example.com/\"},{\"url\":\"https://"
    "example.com/"
    "2\"}],\"tab_groups\":[{\"first_"
    "index\":1,\"last_index\":2,\"title\":\"sample_tab_"
    "group\",\"color\":\"GREY\",\"is_collapsed\":false}],\"active_tab_index\":"
    "1,\"first_non_pinned_tab_index\":1,\"window_id\":0,"
    "\"display_id\":\"100\",\"event_flag\":0}]}}";

inline constexpr char kValidPolicyTemplateBrowserMinimized[] =
    "{\"auto_launch_on_startup\": "
    "false,\"version\":1,\"uuid\":\"040b6112-67f2-4d3c-8ba8-53a117272eba\","
    "\"name\":"
    "\"BrowserTest\",\"created_time_usec\":\"1633535632\",\"updated_time_"
    "usec\": "
    "\"1633535632\",\"desk_type\":\"TEMPLATE\",\"desk\":{\"apps\":[{\"window_"
    "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
    "state\":\"MINIMIZED\",\"z_index\":1,\"app_type\":\"BROWSER\",\"app_id\":"
    "\"mgndgikekgjfcpckkfioiadnlibdjbkf\",\"tabs\":[{"
    "\"url\":\"https://example.com/\"},{\"url\":\"https://"
    "example.com/"
    "2\"}],\"tab_groups\":[{\"first_"
    "index\":1,\"last_index\":2,\"title\":\"sample_tab_"
    "group\",\"color\":\"GREY\",\"is_collapsed\":false}],\"active_tab_index\":"
    "1,\"first_non_pinned_tab_index\":1,\"window_id\":0,"
    "\"display_id\":\"100\",\"event_flag\":0,\"pre_minimized_window_state\":"
    "\"NORMAL\"}]}}";

inline constexpr char kValidPolicyTemplateChromeAndProgressive[] =
    "{\"auto_launch_on_startup\": "
    "false,\"version\":1,\"uuid\":\"7f4b7ff0-970a-41bb-aa91-f6c3e2724207\","
    "\"name\":"
    "\"ChromeAppTest\",\"created_time_usec\":\"1633535632000\",\"updated_time_"
    "usec\": "
    "\"1633535632\",\"desk_type\":\"SAVE_AND_RECALL\",\"desk\":{\"apps\":[{"
    "\"window_"
    "bound\":{"
    "\"left\":200,\"top\":200,\"height\":1000,\"width\":1000},\"window_state\":"
    "\"PRIMARY_SNAPPED\",\"z_index\":2,\"app_type\":\"CHROME_APP\",\"app_id\":"
    "\"test_chrome_app_1\",\"window_id\":0,\"display_id\":\"100\",\"event_"
    "flag\":0, "
    "\"snap_percent\":75,\"override_url\":\"https://example.com/\"},{\"window_"
    "bound\":{\"left\":0,\"top\":0,\"height\":120,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"CHROME_APP\",\"app_id\":"
    "\"test_pwa_app_1\",\"window_id\":1,\"display_id\":\"100\",\"event_flag\":"
    "0,\"override_url\":\"https://example.com/\"}]}}";

inline constexpr char kValidPolicyTemplateChromeForFloatingWorkspace[] =
    "{\"auto_launch_on_startup\": "
    "false,\"version\":1,\"uuid\":\"7f4b7ff0-970a-41bb-aa91-f6c3e2724207\","
    "\"name\":"
    "\"FloatingWorkspaceChromeAppTest\",\"created_time_usec\":"
    "\"1633535632000\",\"updated_time_"
    "usec\": "
    "\"1633535632\",\"desk_type\":\"FLOATING_WORKSPACE\",\"desk\":{\"apps\":[{"
    "\"window_"
    "bound\":{"
    "\"left\":200,\"top\":200,\"height\":1000,\"width\":1000},\"window_state\":"
    "\"PRIMARY_SNAPPED\",\"z_index\":2,\"app_type\":\"CHROME_APP\",\"app_id\":"
    "\"test_chrome_app_1\",\"window_id\":0,\"display_id\":\"100\",\"event_"
    "flag\":0, "
    "\"snap_percent\":75},{\"window_"
    "bound\":{\"left\":0,\"top\":0,\"height\":120,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"CHROME_APP\",\"app_id\":"
    "\"test_pwa_app_1\",\"window_id\":1,\"display_id\":\"100\",\"event_flag\":"
    "0}]}}";

inline constexpr char kPolicyTemplateWithoutType[] =
    "{\"auto_launch_on_startup\": "
    "false,\"version\":1,\"uuid\":\"040b6112-67f2-4d3c-8ba8-53a117272eba\","
    "\"name\":"
    "\"BrowserTest\",\"created_time_usec\":\"1633535632\",\"updated_time_"
    "usec\": "
    "\"1633535632\",\"desk\":{\"apps\":[{\"window_"
    "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
    "\"url\":\"https://example.com/\"},{\"url\":\"https://example.com/"
    "\"}],\"tab_groups\":[{\"first_"
    "index\":1,\"last_index\":2,\"title\":\"sample_tab_"
    "group\",\"color\":\"GREY\",\"is_collapsed\":false}],\"active_tab_index\":"
    "1,\"first_non_pinned_tab_index\":1,\"window_id\":0,"
    "\"display_id\":\"100\",\"event_flag\":0}]}}";

inline constexpr char kAdminTemplatePolicy[] =
    "[{\"auto_launch_on_startup\": "
    "true,\"created_time_usec\": \"13320917261678808\",\"desk\": {\"apps\":  "
    "[{\"app_type\": \"browser\",\"browser_tabs\": [{\"url\": "
    "\"https://www.chromium.org/\"}],\"window_id\": 3000}, {\"app_type\": "
    "\"browser\",\"browser_tabs\": [{\"url\": \"chrome://version/\"},"
    "{\"url\": \"https://dev.chromium.org/\"}],\"window_id\": 30001}]},"
    "\"name\": \"App Launch Automation 1\",\"updated_time_usec\":"
    " \"13320917261678808\",\"uuid\": "
    "\"27ea906b-a7d3-40b1-8c36-76d332d7f184\"},{\"auto_launch_on_startup\":"
    " false,\"created_time_usec\": \"13320917271679905\",\"desk\": {\"apps\": "
    "[{\"app_type\": \"browser\",\"browser_tabs\": "
    "[{\"url\": \"https://www.google.com/\"}, {\"url\": "
    "\"https://www.youtube.com/\"}],\"window_id\": 30001}]},\"name\":"
    " \"App Launch Automation 2\",\"updated_time_usec\":"
    " \"13320917271679905\",\"uuid\":"
    " \"3aa30d88-576e-48ea-ab26-cbdd2cbe43a1\"}]";

inline constexpr char kAdminTemplatePolicyWithOneTemplate[] =
    "[{\"auto_launch_on_startup\": "
    "true,\"created_time_usec\": \"13320917261678808\",\"desk\": {\"apps\":  "
    "[{\"app_type\": \"browser\",\"browser_tabs\": [{\"url\": "
    "\"https://www.chromium.org/\"}],\"window_id\": 3000}, {\"app_type\": "
    "\"browser\",\"browser_tabs\": [{\"url\": \"chrome://version/\"},"
    "{\"url\": \"https://dev.chromium.org/\"}],\"window_id\": 30001}]},"
    "\"name\": \"App Launch Automation 1\",\"updated_time_usec\":"
    " \"13320917261678808\",\"uuid\": "
    "\"27ea906b-a7d3-40b1-8c36-76d332d7f184\"}]";

// Populates the given cache with test app information.
void PopulateAppRegistryCache(AccountId account_id,
                              apps::AppRegistryCache* cache);

void AddAppIdToAppRegistryCache(AccountId account_id,
                                apps::AppRegistryCache* cache,
                                const char* app_id);

// Populates browser apps and notifies cache observers. Note: This app assumes
// that `cache` has been added to the AppRegistryCacheWrapper already.
void PopulateAdminTestAppRegistryCache(AccountId account_id,
                                       apps::AppRegistryCache* cache);

// Similar to `PopulateAdminTestAppRegistryCache`, but includes web apps.
void PopulateFloatingWorkspaceAppRegistryCache(AccountId account_id,
                                               apps::AppRegistryCache* cache);

}  // namespace desks_storage::desk_test_util

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_TEST_UTIL_H_
