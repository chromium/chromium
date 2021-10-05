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
  for (int i = Source::kMinValue; i <= Source::kMaxValue; ++i) {
    app.AddSource(static_cast<Source::Type>(i));
    EXPECT_TRUE(app.HasAnySources());
  }

  for (int i = Source::kMinValue; i <= Source::kMaxValue; ++i) {
    EXPECT_TRUE(app.HasAnySources());
    app.RemoveSource(static_cast<Source::Type>(i));
  }
  EXPECT_FALSE(app.HasAnySources());
}

TEST(WebAppTest, HasOnlySource) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

  for (int i = Source::kMinValue; i <= Source::kMaxValue; ++i) {
    auto source = static_cast<Source::Type>(i);

    app.AddSource(source);
    EXPECT_TRUE(app.HasOnlySource(source));

    app.RemoveSource(source);
    EXPECT_FALSE(app.HasOnlySource(source));
  }

  app.AddSource(Source::kMinValue);
  EXPECT_TRUE(app.HasOnlySource(Source::kMinValue));

  for (int i = Source::kMinValue + 1; i <= Source::kMaxValue; ++i) {
    auto source = static_cast<Source::Type>(i);
    app.AddSource(source);
    EXPECT_FALSE(app.HasOnlySource(source));
    EXPECT_FALSE(app.HasOnlySource(Source::kMinValue));
  }

  for (int i = Source::kMinValue + 1; i <= Source::kMaxValue; ++i) {
    auto source = static_cast<Source::Type>(i);
    EXPECT_FALSE(app.HasOnlySource(Source::kMinValue));
    app.RemoveSource(source);
    EXPECT_FALSE(app.HasOnlySource(source));
  }

  EXPECT_TRUE(app.HasOnlySource(Source::kMinValue));
  app.RemoveSource(Source::kMinValue);
  EXPECT_FALSE(app.HasOnlySource(Source::kMinValue));
  EXPECT_FALSE(app.HasAnySources());
}

TEST(WebAppTest, WasInstalledByUser) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

  app.AddSource(Source::kSync);
  EXPECT_TRUE(app.WasInstalledByUser());

  app.AddSource(Source::kWebAppStore);
  EXPECT_TRUE(app.WasInstalledByUser());

  app.RemoveSource(Source::kSync);
  EXPECT_TRUE(app.WasInstalledByUser());

  app.RemoveSource(Source::kWebAppStore);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(Source::kDefault);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(Source::kSystem);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(Source::kPolicy);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(Source::kDefault);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(Source::kSystem);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(Source::kPolicy);
  EXPECT_FALSE(app.WasInstalledByUser());
}

TEST(WebAppTest, CanUserUninstallWebApp) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

  app.AddSource(Source::kDefault);
  EXPECT_TRUE(app.IsPreinstalledApp());
  EXPECT_TRUE(app.CanUserUninstallWebApp());

  app.AddSource(Source::kSync);
  EXPECT_TRUE(app.CanUserUninstallWebApp());
  app.AddSource(Source::kWebAppStore);
  EXPECT_TRUE(app.CanUserUninstallWebApp());

  app.AddSource(Source::kPolicy);
  EXPECT_FALSE(app.CanUserUninstallWebApp());
  app.AddSource(Source::kSystem);
  EXPECT_FALSE(app.CanUserUninstallWebApp());

  app.RemoveSource(Source::kSync);
  EXPECT_FALSE(app.CanUserUninstallWebApp());
  app.RemoveSource(Source::kWebAppStore);
  EXPECT_FALSE(app.CanUserUninstallWebApp());

  app.RemoveSource(Source::kSystem);
  EXPECT_FALSE(app.CanUserUninstallWebApp());

  app.RemoveSource(Source::kPolicy);
  EXPECT_TRUE(app.CanUserUninstallWebApp());

  EXPECT_TRUE(app.IsPreinstalledApp());
  app.RemoveSource(Source::kDefault);
  EXPECT_FALSE(app.IsPreinstalledApp());
}

TEST(WebAppTest, EmptyAppAsDebugValue) {
  EXPECT_EQ(
      base::JSONReader::Read(R"({
   "!app_id": "empty_app",
   "!name": "",
   "additional_search_terms": [  ],
   "app_service_icon_url": "chrome://app-icon/empty_app/32",
   "allowed_launch_protocols": [  ],
   "background_color": "none",
   "dark_mode_theme_color": "none",
   "capture_links": "kUndefined",
   "chromeos_data": null,
   "client_data": {
      "system_web_app_data": null
   },
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
   "file_handler_permission_blocked": false,
   "file_handlers": [  ],
   "manifest_icons": [  ],
   "install_time": "1601-01-01 00:00:00.000 UTC",
   "is_generated_icon": false,
   "is_from_sync_and_pending_installation": false,
   "is_locally_installed": true,
   "is_storage_isolated": false,
   "is_uninstalling": false,
   "last_badging_time": "1601-01-01 00:00:00.000 UTC",
   "last_launch_time": "1601-01-01 00:00:00.000 UTC",
   "launch_handler": null,
   "launch_query_params": null,
   "manifest_id": null,
   "manifest_update_time": "1601-01-01 00:00:00.000 UTC",
   "manifest_url": "",
   "note_taking_new_note_url": "",
   "protocol_handlers": [  ],
   "run_on_os_login_mode": "not run",
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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EnableSystemWebAppsInLacrosForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  EXPECT_EQ(base::JSONReader::Read(R"JSON({
   "!app_id": "eajjdjobhihlgobdfaehiiheinneagde",
   "!name": "Name1234",
   "additional_search_terms": [ "Foo_1234_0", "Foo_1234_1" ],
   "allowed_launch_protocols": [ "web+test_1234_0", "web+test_1234_1", "web+test_1234_2", "web+test_1234_3" ],
   "app_service_icon_url": "chrome://app-icon/eajjdjobhihlgobdfaehiiheinneagde/32",
   "background_color": "rgba(77,188,194,0.9686274509803922)",
   "capture_links": "kNewClient",
   "chromeos_data": null,
   "client_data": {
      "system_web_app_data": null
   },
   "dark_mode_theme_color": "rgba(89,101,0,1)",
   "description": "Description1234",
   "disallowed_launch_protocols": [ "web+disallowed_1234_0", "web+disallowed_1234_1", "web+disallowed_1234_2", "web+disallowed_1234_3", "web+disallowed_1234_4", "web+disallowed_1234_5", "web+disallowed_1234_6" ],
   "display_mode": "fullscreen",
   "display_override": [  ],
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
      "ANY": [ 38 ],
      "MASKABLE": [ 228 ],
      "MONOCHROME": [ 80 ],
      "index": 1
   }, {
      "ANY": [ 240, 164 ],
      "MASKABLE": [ 138, 107 ],
      "MONOCHROME": [ 47, 3 ],
      "index": 2
   } ],
   "file_handler_permission_blocked": false,
   "file_handlers": [ {
      "accept": [ {
         "file_extensions": [ ".244307310a", ".244307310b" ],
         "mime_type": "application/244307310+foo"
      }, {
         "file_extensions": [ ".244307310a", ".244307310b" ],
         "mime_type": "application/244307310+bar"
      } ],
      "action": "https://example.com/open-244307310",
      "downloaded_icons": [ {
         "purpose": "kAny",
         "square_size_px": 16,
         "url": "https://example.com/image.png"
      }, {
         "purpose": "kAny",
         "square_size_px": 48,
         "url": "https://example.com/image2.png"
      } ],
      "name": "244307310 file"
   }, {
      "accept": [ {
         "file_extensions": [ ".244307311a", ".244307311b" ],
         "mime_type": "application/244307311+foo"
      }, {
         "file_extensions": [ ".244307311a", ".244307311b" ],
         "mime_type": "application/244307311+bar"
      } ],
      "action": "https://example.com/open-244307311",
      "downloaded_icons": [ {
         "purpose": "kAny",
         "square_size_px": 16,
         "url": "https://example.com/image.png"
      }, {
         "purpose": "kAny",
         "square_size_px": 48,
         "url": "https://example.com/image2.png"
      } ],
      "name": "244307311 file"
   }, {
      "accept": [ {
         "file_extensions": [ ".244307312a", ".244307312b" ],
         "mime_type": "application/244307312+foo"
      }, {
         "file_extensions": [ ".244307312a", ".244307312b" ],
         "mime_type": "application/244307312+bar"
      } ],
      "action": "https://example.com/open-244307312",
      "downloaded_icons": [ {
         "purpose": "kAny",
         "square_size_px": 16,
         "url": "https://example.com/image.png"
      }, {
         "purpose": "kAny",
         "square_size_px": 48,
         "url": "https://example.com/image2.png"
      } ],
      "name": "244307312 file"
   }, {
      "accept": [ {
         "file_extensions": [ ".244307313a", ".244307313b" ],
         "mime_type": "application/244307313+foo"
      }, {
         "file_extensions": [ ".244307313a", ".244307313b" ],
         "mime_type": "application/244307313+bar"
      } ],
      "action": "https://example.com/open-244307313",
      "downloaded_icons": [ {
         "purpose": "kAny",
         "square_size_px": 16,
         "url": "https://example.com/image.png"
      }, {
         "purpose": "kAny",
         "square_size_px": 48,
         "url": "https://example.com/image2.png"
      } ],
      "name": "244307313 file"
   }, {
      "accept": [ {
         "file_extensions": [ ".244307314a", ".244307314b" ],
         "mime_type": "application/244307314+foo"
      }, {
         "file_extensions": [ ".244307314a", ".244307314b" ],
         "mime_type": "application/244307314+bar"
      } ],
      "action": "https://example.com/open-244307314",
      "downloaded_icons": [ {
         "purpose": "kAny",
         "square_size_px": 16,
         "url": "https://example.com/image.png"
      }, {
         "purpose": "kAny",
         "square_size_px": 48,
         "url": "https://example.com/image2.png"
      } ],
      "name": "244307314 file"
   } ],
   "install_time": "1970-01-04 17:38:34.900 UTC",
   "is_from_sync_and_pending_installation": false,
   "is_generated_icon": false,
   "is_locally_installed": false,
   "is_storage_isolated": true,
   "is_uninstalling": false,
   "last_badging_time": "1970-01-09 06:11:52.363 UTC",
   "last_launch_time": "1970-01-13 20:12:59.451 UTC",
   "launch_handler": {
      "navigate_existing_client": "kAlways",
      "route_to": "kAuto"
   },
   "launch_query_params": "1267856662",
   "manifest_icons": [ {
      "purpose": "kMonochrome",
      "square_size_px": null,
      "url": "https://example.com/icon1739605659"
   }, {
      "purpose": "kAny",
      "square_size_px": 256,
      "url": "https://example.com/icon3209113155"
   } ],
   "manifest_id": null,
   "manifest_update_time": "1970-01-13 21:07:13.914 UTC",
   "manifest_url": "https://example.com/manifest1234.json",
   "note_taking_new_note_url": "",
   "protocol_handlers": [ {
      "protocol": "web+test15459094040",
      "url": "https://example.com/15459094040"
   }, {
      "protocol": "web+test15459094041",
      "url": "https://example.com/15459094041"
   }, {
      "protocol": "web+test15459094042",
      "url": "https://example.com/15459094042"
   }, {
      "protocol": "web+test15459094043",
      "url": "https://example.com/15459094043"
   }, {
      "protocol": "web+test15459094044",
      "url": "https://example.com/15459094044"
   } ],
   "run_on_os_login_mode": "windowed",
   "scope": "https://example.com/scope1234/",
   "share_target": {
      "action": "https://example.com/path/target/259117484",
      "enctype": "multipart/form-data",
      "method": "POST",
      "params": {
         "files": [ {
            "accept": [ ".extension0", "type/subtype0" ],
            "name": "files0"
         }, {
            "accept": [ ".extension1", "type/subtype1" ],
            "name": "files1"
         }, {
            "accept": [ ".extension2", "type/subtype2" ],
            "name": "files2"
         }, {
            "accept": [ ".extension3", "type/subtype3" ],
            "name": "files3"
         } ],
         "text": "text259117484",
         "title": "title259117484",
         "url": ""
      }
   },
   "shortcuts_menu_item_infos": [ {
      "icons": {
         "ANY": [  ],
         "MASKABLE": [ {
            "square_size_px": 10,
            "url": "https://example.com/shortcuts/icon246211516021"
         }, {
            "square_size_px": 1,
            "url": "https://example.com/shortcuts/icon246211516020"
         } ],
         "MONOCHROME": [ {
            "square_size_px": 21,
            "url": "https://example.com/shortcuts/icon246211516022"
         } ]
      },
      "name": "shortcut24621151602",
      "url": "https://example.com/scope1234/shortcut24621151602"
   }, {
      "icons": {
         "ANY": [ {
            "square_size_px": 21,
            "url": "https://example.com/shortcuts/icon246211516012"
         } ],
         "MASKABLE": [ {
            "square_size_px": 33,
            "url": "https://example.com/shortcuts/icon246211516013"
         }, {
            "square_size_px": 12,
            "url": "https://example.com/shortcuts/icon246211516011"
         } ],
         "MONOCHROME": [ {
            "square_size_px": 3,
            "url": "https://example.com/shortcuts/icon246211516010"
         } ]
      },
      "name": "shortcut24621151601",
      "url": "https://example.com/scope1234/shortcut24621151601"
   }, {
      "icons": {
         "ANY": [ {
            "square_size_px": 15,
            "url": "https://example.com/shortcuts/icon246211516001"
         } ],
         "MASKABLE": [  ],
         "MONOCHROME": [ {
            "square_size_px": 1,
            "url": "https://example.com/shortcuts/icon246211516000"
         } ]
      },
      "name": "shortcut24621151600",
      "url": "https://example.com/scope1234/shortcut24621151600"
   } ],
   "sources": [ "WebAppStore", "Sync", "Default" ],
   "start_url": "https://example.com/scope1234/start1234",
   "sync_fallback_data": {
      "manifest_icons": [ {
         "purpose": "kMonochrome",
         "square_size_px": null,
         "url": "https://example.com/icon1739605659"
      }, {
         "purpose": "kAny",
         "square_size_px": 256,
         "url": "https://example.com/icon3209113155"
      } ],
      "name": "SyncName1234",
      "scope": "https://example.com/scope1234/",
      "theme_color": "rgba(61,127,69,0.8431372549019608)"
   },
   "theme_color": "rgba(151,34,83,0.8823529411764706)",
   "unhashed_app_id": "https://example.com/scope1234/start1234",
   "url_handlers": [ {
      "exclude_paths": [  ],
      "has_origin_wildcard": true,
      "origin": "https://app-24741963850.com",
      "paths": [  ]
   }, {
      "exclude_paths": [  ],
      "has_origin_wildcard": true,
      "origin": "https://app-24741963851.com",
      "paths": [  ]
   }, {
      "exclude_paths": [  ],
      "has_origin_wildcard": true,
      "origin": "https://app-24741963852.com",
      "paths": [  ]
   } ],
   "user_display_mode": "tabbed",
   "user_launch_ordinal": "INVALID[]",
   "user_page_ordinal": "INVALID[]",
   "window_controls_overlay_enabled": false
})JSON")
                .value_or(base::Value("Failed to parse")),
            WebAppToPlatformAgnosticJson(test::CreateRandomWebApp(
                GURL("https://example.com/"), /*seed=*/1234)));
}

}  // namespace web_app
