// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include <string>

#include "base/json/json_reader.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

base::Value WebAppToPlatformAgnosticJson(std::unique_ptr<WebApp> web_app) {
  // Force this to be nullopt to avoid platform specific differences.
  web_app->SetWebAppChromeOsData(absl::nullopt);
  return web_app->AsDebugValue();
}

}  // namespace

TEST(WebAppTest, HasAnySources) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

  EXPECT_FALSE(app.HasAnySources());
  for (int i = WebAppManagement::kMinValue; i <= WebAppManagement::kMaxValue;
       ++i) {
    app.AddSource(static_cast<WebAppManagement::Type>(i));
    EXPECT_TRUE(app.HasAnySources());
  }

  for (int i = WebAppManagement::kMinValue; i <= WebAppManagement::kMaxValue;
       ++i) {
    EXPECT_TRUE(app.HasAnySources());
    app.RemoveSource(static_cast<WebAppManagement::Type>(i));
  }
  EXPECT_FALSE(app.HasAnySources());
}

TEST(WebAppTest, HasOnlySource) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

  for (int i = WebAppManagement::kMinValue; i <= WebAppManagement::kMaxValue;
       ++i) {
    auto source = static_cast<WebAppManagement::Type>(i);

    app.AddSource(source);
    EXPECT_TRUE(app.HasOnlySource(source));

    app.RemoveSource(source);
    EXPECT_FALSE(app.HasOnlySource(source));
  }

  app.AddSource(WebAppManagement::kMinValue);
  EXPECT_TRUE(app.HasOnlySource(WebAppManagement::kMinValue));

  for (int i = WebAppManagement::kMinValue + 1;
       i <= WebAppManagement::kMaxValue; ++i) {
    auto source = static_cast<WebAppManagement::Type>(i);
    app.AddSource(source);
    EXPECT_FALSE(app.HasOnlySource(source));
    EXPECT_FALSE(app.HasOnlySource(WebAppManagement::kMinValue));
  }

  for (int i = WebAppManagement::kMinValue + 1;
       i <= WebAppManagement::kMaxValue; ++i) {
    auto source = static_cast<WebAppManagement::Type>(i);
    EXPECT_FALSE(app.HasOnlySource(WebAppManagement::kMinValue));
    app.RemoveSource(source);
    EXPECT_FALSE(app.HasOnlySource(source));
  }

  EXPECT_TRUE(app.HasOnlySource(WebAppManagement::kMinValue));
  app.RemoveSource(WebAppManagement::kMinValue);
  EXPECT_FALSE(app.HasOnlySource(WebAppManagement::kMinValue));
  EXPECT_FALSE(app.HasAnySources());
}

TEST(WebAppTest, WasInstalledByUser) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

  app.AddSource(WebAppManagement::kSync);
  EXPECT_TRUE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kWebAppStore);
  EXPECT_TRUE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kSync);
  EXPECT_TRUE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kWebAppStore);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kDefault);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kSystem);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kPolicy);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kSubApp);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kDefault);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kSystem);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kPolicy);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kSubApp);
  EXPECT_FALSE(app.WasInstalledByUser());
}

TEST(WebAppTest, CanUserUninstallWebApp) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

  app.AddSource(WebAppManagement::kDefault);
  EXPECT_TRUE(app.IsPreinstalledApp());
  EXPECT_TRUE(app.CanUserUninstallWebApp());

  app.AddSource(WebAppManagement::kSync);
  EXPECT_TRUE(app.CanUserUninstallWebApp());
  app.AddSource(WebAppManagement::kWebAppStore);
  EXPECT_TRUE(app.CanUserUninstallWebApp());
  app.AddSource(WebAppManagement::kSubApp);
  EXPECT_TRUE(app.CanUserUninstallWebApp());

  app.AddSource(WebAppManagement::kPolicy);
  EXPECT_FALSE(app.CanUserUninstallWebApp());
  app.AddSource(WebAppManagement::kSystem);
  EXPECT_FALSE(app.CanUserUninstallWebApp());

  app.RemoveSource(WebAppManagement::kSync);
  EXPECT_FALSE(app.CanUserUninstallWebApp());
  app.RemoveSource(WebAppManagement::kWebAppStore);
  EXPECT_FALSE(app.CanUserUninstallWebApp());
  app.RemoveSource(WebAppManagement::kSubApp);
  EXPECT_FALSE(app.CanUserUninstallWebApp());

  app.RemoveSource(WebAppManagement::kSystem);
  EXPECT_FALSE(app.CanUserUninstallWebApp());

  app.RemoveSource(WebAppManagement::kPolicy);
  EXPECT_TRUE(app.CanUserUninstallWebApp());

  EXPECT_TRUE(app.IsPreinstalledApp());
  app.RemoveSource(WebAppManagement::kDefault);
  EXPECT_FALSE(app.IsPreinstalledApp());
}

TEST(WebAppTest, EmptyAppAsDebugValue) {
  EXPECT_EQ(
      base::JSONReader::Read(R"({
   "!app_id": "empty_app",
   "!name": "",
   "additional_search_terms": [  ],
   "allowed_launch_protocols": [  ],
   "app_service_icon_url": "chrome://app-icon/empty_app/32",
   "app_size_in_bytes": "",
   "background_color": "none",
   "capture_links": "kUndefined",
   "chromeos_data": null,
   "client_data": {
      "system_web_app_data": null
   },
   "dark_mode_background_color": "none",
   "dark_mode_theme_color": "none",
   "data_size_in_bytes": "",
   "description": "",
   "disallowed_launch_protocols": [  ],
   "display_mode": "",
   "display_override": [  ],
   "downloaded_icon_sizes": {
      "ANY": [  ],
      "MASKABLE": [  ],
      "MONOCHROME": [  ]
   },
   "downloaded_shortcuts_menu_icons_sizes": [  ],
   "file_handler_approval_state": "kRequiresPrompt",
   "file_handler_os_integration_state": "kDisabled",
   "file_handlers": [  ],
   "install_source_for_metrics": "not set",
   "install_time": "1601-01-01 00:00:00.000 UTC",
   "is_from_sync_and_pending_installation": false,
   "is_generated_icon": false,
   "is_locally_installed": true,
   "is_storage_isolated": false,
   "is_uninstalling": false,
   "last_badging_time": "1601-01-01 00:00:00.000 UTC",
   "last_launch_time": "1601-01-01 00:00:00.000 UTC",
   "launch_handler": null,
   "launch_query_params": null,
   "lock_screen_start_url": "",
   "management_type_to_external_configuration_map": {
   },
   "manifest_icons": [  ],
   "manifest_id": null,
   "manifest_update_time": "1601-01-01 00:00:00.000 UTC",
   "manifest_url": "",
   "note_taking_new_note_url": "",
   "parent_app_id": "",
   "protocol_handlers": [  ],
   "run_on_os_login_mode": "not run",
   "run_on_os_login_os_integration_state": "not set",
   "scope": "",
   "share_target": null,
   "shortcuts_menu_item_infos": [  ],
   "sources": [  ],
   "start_url": "",
   "sync_fallback_data": {
      "manifest_icons": [  ],
      "name": "",
      "scope": "",
      "theme_color": "none"
   },
   "tab_strip": null,
   "theme_color": "none",
   "unhashed_app_id": "",
   "url_handlers": [  ],
   "user_display_mode": "",
   "user_launch_ordinal": "INVALID[]",
   "user_page_ordinal": "INVALID[]",
   "window_controls_overlay_enabled": false
})")
          .value_or(base::Value("Failed to parse")),
      WebAppToPlatformAgnosticJson(std::make_unique<WebApp>("empty_app")));
}

TEST(WebAppTest, SampleAppAsDebugValue) {
  EXPECT_EQ(base::JSONReader::Read(R"JSON({
   "!app_id": "eajjdjobhihlgobdfaehiiheinneagde",
   "!name": "Name1234",
   "additional_search_terms": [ "Foo_1234_0" ],
   "allowed_launch_protocols": [ "web+test_1234_0", "web+test_1234_1" ],
   "app_service_icon_url": "chrome://app-icon/eajjdjobhihlgobdfaehiiheinneagde/32",
   "app_size_in_bytes": "4226285750",
   "background_color": "rgba(77,188,194,0.9686274509803922)",
   "capture_links": "kNone",
   "chromeos_data": null,
   "client_data": {
      "system_web_app_data": null
   },
   "dark_mode_background_color": "none",
   "dark_mode_theme_color": "none",
   "data_size_in_bytes": "3687618762",
   "description": "Description1234",
   "disallowed_launch_protocols": [ "web+disallowed_1234_0", "web+disallowed_1234_1", "web+disallowed_1234_2", "web+disallowed_1234_3" ],
   "display_mode": "standalone",
   "display_override": [ "standalone" ],
   "downloaded_icon_sizes": {
      "ANY": [ 256 ],
      "MASKABLE": [  ],
      "MONOCHROME": [ 256 ]
   },
   "downloaded_shortcuts_menu_icons_sizes": [ {
      "ANY": [  ],
      "MASKABLE": [  ],
      "MONOCHROME": [  ],
      "index": 0
   }, {
      "ANY": [ 118 ],
      "MASKABLE": [ 38 ],
      "MONOCHROME": [ 228 ],
      "index": 1
   }, {
      "ANY": [ 80, 47 ],
      "MASKABLE": [ 240, 164 ],
      "MONOCHROME": [ 138, 107 ],
      "index": 2
   } ],
   "file_handler_approval_state": "kRequiresPrompt",
   "file_handler_os_integration_state": "kDisabled",
   "file_handlers": [ {
      "accept": [ {
         "file_extensions": [ ".2591174840a", ".2591174840b" ],
         "mime_type": "application/2591174840+foo"
      }, {
         "file_extensions": [ ".2591174840a", ".2591174840b" ],
         "mime_type": "application/2591174840+bar"
      } ],
      "action": "https://example.com/open-2591174840",
      "downloaded_icons": [ {
         "purpose": "kAny",
         "square_size_px": 16,
         "url": "https://example.com/image.png"
      }, {
         "purpose": "kAny",
         "square_size_px": 48,
         "url": "https://example.com/image2.png"
      } ],
      "launch_type": "kSingleClient",
      "name": "2591174840 file"
   }, {
      "accept": [ {
         "file_extensions": [ ".2591174841a", ".2591174841b" ],
         "mime_type": "application/2591174841+foo"
      }, {
         "file_extensions": [ ".2591174841a", ".2591174841b" ],
         "mime_type": "application/2591174841+bar"
      } ],
      "action": "https://example.com/open-2591174841",
      "downloaded_icons": [ {
         "purpose": "kAny",
         "square_size_px": 16,
         "url": "https://example.com/image.png"
      }, {
         "purpose": "kAny",
         "square_size_px": 48,
         "url": "https://example.com/image2.png"
      } ],
      "launch_type": "kSingleClient",
      "name": "2591174841 file"
   }, {
      "accept": [ {
         "file_extensions": [ ".2591174842a", ".2591174842b" ],
         "mime_type": "application/2591174842+foo"
      }, {
         "file_extensions": [ ".2591174842a", ".2591174842b" ],
         "mime_type": "application/2591174842+bar"
      } ],
      "action": "https://example.com/open-2591174842",
      "downloaded_icons": [ {
         "purpose": "kAny",
         "square_size_px": 16,
         "url": "https://example.com/image.png"
      }, {
         "purpose": "kAny",
         "square_size_px": 48,
         "url": "https://example.com/image2.png"
      } ],
      "launch_type": "kSingleClient",
      "name": "2591174842 file"
   }, {
      "accept": [ {
         "file_extensions": [ ".2591174843a", ".2591174843b" ],
         "mime_type": "application/2591174843+foo"
      }, {
         "file_extensions": [ ".2591174843a", ".2591174843b" ],
         "mime_type": "application/2591174843+bar"
      } ],
      "action": "https://example.com/open-2591174843",
      "downloaded_icons": [ {
         "purpose": "kAny",
         "square_size_px": 16,
         "url": "https://example.com/image.png"
      }, {
         "purpose": "kAny",
         "square_size_px": 48,
         "url": "https://example.com/image2.png"
      } ],
      "launch_type": "kSingleClient",
      "name": "2591174843 file"
   }, {
      "accept": [ {
         "file_extensions": [ ".2591174844a", ".2591174844b" ],
         "mime_type": "application/2591174844+foo"
      }, {
         "file_extensions": [ ".2591174844a", ".2591174844b" ],
         "mime_type": "application/2591174844+bar"
      } ],
      "action": "https://example.com/open-2591174844",
      "downloaded_icons": [ {
         "purpose": "kAny",
         "square_size_px": 16,
         "url": "https://example.com/image.png"
      }, {
         "purpose": "kAny",
         "square_size_px": 48,
         "url": "https://example.com/image2.png"
      } ],
      "launch_type": "kSingleClient",
      "name": "2591174844 file"
   } ],
   "install_source_for_metrics": 17,
   "install_time": "1970-01-10 21:57:36.131 UTC",
   "is_from_sync_and_pending_installation": false,
   "is_generated_icon": true,
   "is_locally_installed": false,
   "is_storage_isolated": true,
   "is_uninstalling": false,
   "last_badging_time": "1970-01-13 20:12:59.451 UTC",
   "last_launch_time": "1970-01-04 17:38:34.900 UTC",
   "launch_handler": {
      "route_to": "kExistingClientNavigate"
   },
   "launch_query_params": "986688382",
   "lock_screen_start_url": "https://example.com/scope1234/lock_screen_start_url3206632378",
   "management_type_to_external_configuration_map": {
      "Default": {
         "install_urls": [ "https://example.com/installer1_1234/", "https://example.com/installer2_1234/" ],
         "is_placeholder": false
      },
      "SubApp": {
         "install_urls": [ "https://example.com/installer1_1234/" ],
         "is_placeholder": true
      },
      "WebAppStore": {
         "install_urls": [ "https://example.com/installer1_1234/", "https://example.com/installer2_1234/" ],
         "is_placeholder": false
      }
   },
   "manifest_icons": [ {
      "purpose": "kAny",
      "square_size_px": 256,
      "url": "https://example.com/icon2077353522"
   }, {
      "purpose": "kAny",
      "square_size_px": 256,
      "url": "https://example.com/icon944292860"
   } ],
   "manifest_id": null,
   "manifest_update_time": "1970-01-21 01:09:01.170 UTC",
   "manifest_url": "https://example.com/manifest1234.json",
   "note_taking_new_note_url": "",
   "parent_app_id": "1112833914",
   "permissions_policy": [ {
      "allowed_origins": [ "https://app-1994259750.com", "https://app-1994259751.com", "https://app-1994259752.com", "https://app-1994259753.com", "https://app-1994259754.com" ],
      "feature": "accelerometer",
      "matches_all_origins": false,
      "matches_opaque_src": false
   }, {
      "allowed_origins": [ "https://app-1994259750.com", "https://app-1994259751.com", "https://app-1994259752.com", "https://app-1994259753.com", "https://app-1994259754.com" ],
      "feature": "accelerometer",
      "matches_all_origins": false,
      "matches_opaque_src": false
   } ],
   "protocol_handlers": [ {
      "protocol": "web+test24741963850",
      "url": "https://example.com/24741963850"
   }, {
      "protocol": "web+test24741963851",
      "url": "https://example.com/24741963851"
   }, {
      "protocol": "web+test24741963852",
      "url": "https://example.com/24741963852"
   }, {
      "protocol": "web+test24741963853",
      "url": "https://example.com/24741963853"
   }, {
      "protocol": "web+test24741963854",
      "url": "https://example.com/24741963854"
   } ],
   "run_on_os_login_mode": "windowed",
   "run_on_os_login_os_integration_state": "not run",
   "scope": "https://example.com/scope1234/",
   "share_target": null,
   "shortcuts_menu_item_infos": [ {
      "icons": {
         "ANY": [  ],
         "MASKABLE": [ {
            "square_size_px": 9,
            "url": "https://example.com/shortcuts/icon302299027120"
         } ],
         "MONOCHROME": [ {
            "square_size_px": 18,
            "url": "https://example.com/shortcuts/icon302299027121"
         } ]
      },
      "name": "shortcut30229902712",
      "url": "https://example.com/scope1234/shortcut30229902712"
   }, {
      "icons": {
         "ANY": [ {
            "square_size_px": 14,
            "url": "https://example.com/shortcuts/icon302299027111"
         } ],
         "MASKABLE": [ {
            "square_size_px": 29,
            "url": "https://example.com/shortcuts/icon302299027112"
         }, {
            "square_size_px": 7,
            "url": "https://example.com/shortcuts/icon302299027110"
         } ],
         "MONOCHROME": [  ]
      },
      "name": "shortcut30229902711",
      "url": "https://example.com/scope1234/shortcut30229902711"
   }, {
      "icons": {
         "ANY": [ {
            "square_size_px": 0,
            "url": "https://example.com/shortcuts/icon302299027100"
         } ],
         "MASKABLE": [  ],
         "MONOCHROME": [ {
            "square_size_px": 16,
            "url": "https://example.com/shortcuts/icon302299027101"
         } ]
      },
      "name": "shortcut30229902710",
      "url": "https://example.com/scope1234/shortcut30229902710"
   } ],
   "sources": [ "SubApp", "WebAppStore", "Sync", "Default" ],
   "start_url": "https://example.com/scope1234/start1234",
   "sync_fallback_data": {
      "manifest_icons": [ {
         "purpose": "kAny",
         "square_size_px": 256,
         "url": "https://example.com/icon2077353522"
      }, {
         "purpose": "kAny",
         "square_size_px": 256,
         "url": "https://example.com/icon944292860"
      } ],
      "name": "SyncName1234",
      "scope": "https://example.com/scope1234/",
      "theme_color": "rgba(61,127,69,0.8431372549019608)"
   },
   "tab_strip": null,
   "theme_color": "rgba(151,34,83,0.8823529411764706)",
   "unhashed_app_id": "https://example.com/scope1234/start1234",
   "url_handlers": [ {
      "exclude_paths": [  ],
      "has_origin_wildcard": true,
      "origin": "https://app-29001084320.com",
      "paths": [  ]
   }, {
      "exclude_paths": [  ],
      "has_origin_wildcard": true,
      "origin": "https://app-29001084321.com",
      "paths": [  ]
   }, {
      "exclude_paths": [  ],
      "has_origin_wildcard": true,
      "origin": "https://app-29001084322.com",
      "paths": [  ]
   } ],
   "user_display_mode": "standalone",
   "user_launch_ordinal": "INVALID[]",
   "user_page_ordinal": "INVALID[]",
   "window_controls_overlay_enabled": false
})JSON")
                .value_or(base::Value("Failed to parse")),
            WebAppToPlatformAgnosticJson(test::CreateRandomWebApp(
                GURL("https://example.com/"), /*seed=*/1234)));
}

}  // namespace web_app
