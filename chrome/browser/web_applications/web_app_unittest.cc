// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include <string>

#include "base/json/json_reader.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
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
   "approved_launch_protocols": [  ],
   "background_color": "none",
   "capture_links": "kUndefined",
   "chromeos_data": null,
   "client_data": {
      "system_web_app_data": null
   },
   "description": "",
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
   "icon_infos": [  ],
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
      "icon_infos": [  ],
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
  EXPECT_EQ(base::JSONReader::Read(R"JSON({
   "!app_id": "eajjdjobhihlgobdfaehiiheinneagde",
   "!name": "Name1234",
   "additional_search_terms": [ "Foo_1234_0" ],
   "app_service_icon_url": "chrome://app-icon/eajjdjobhihlgobdfaehiiheinneagde/32",
   "approved_launch_protocols": [ "web+test_1234_0", "web+test_1234_1" ],
   "background_color": "rgba(77,188,194,0.9686274509803922)",
   "capture_links": "kNewClient",
   "chromeos_data": null,
   "client_data": {
      "system_web_app_data": null
   },
   "description": "Description1234",
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
   "file_handler_permission_blocked": false,
   "file_handlers": [ {
      "accept": [ {
         "file_extensions": [ ".13087720410a", ".13087720410b" ],
         "mime_type": "application/13087720410+foo"
      }, {
         "file_extensions": [ ".13087720410a", ".13087720410b" ],
         "mime_type": "application/13087720410+bar"
      } ],
      "action": "https://example.com/open-13087720410"
   }, {
      "accept": [ {
         "file_extensions": [ ".13087720411a", ".13087720411b" ],
         "mime_type": "application/13087720411+foo"
      }, {
         "file_extensions": [ ".13087720411a", ".13087720411b" ],
         "mime_type": "application/13087720411+bar"
      } ],
      "action": "https://example.com/open-13087720411"
   }, {
      "accept": [ {
         "file_extensions": [ ".13087720412a", ".13087720412b" ],
         "mime_type": "application/13087720412+foo"
      }, {
         "file_extensions": [ ".13087720412a", ".13087720412b" ],
         "mime_type": "application/13087720412+bar"
      } ],
      "action": "https://example.com/open-13087720412"
   }, {
      "accept": [ {
         "file_extensions": [ ".13087720413a", ".13087720413b" ],
         "mime_type": "application/13087720413+foo"
      }, {
         "file_extensions": [ ".13087720413a", ".13087720413b" ],
         "mime_type": "application/13087720413+bar"
      } ],
      "action": "https://example.com/open-13087720413"
   }, {
      "accept": [ {
         "file_extensions": [ ".13087720414a", ".13087720414b" ],
         "mime_type": "application/13087720414+foo"
      }, {
         "file_extensions": [ ".13087720414a", ".13087720414b" ],
         "mime_type": "application/13087720414+bar"
      } ],
      "action": "https://example.com/open-13087720414"
   } ],
   "icon_infos": [ {
      "purpose": "ANY",
      "square_size_px": null,
      "url": "https://example.com/icon1783899413"
   }, {
      "purpose": "ANY",
      "square_size_px": null,
      "url": "https://example.com/icon3011162902"
   } ],
   "install_time": "1970-01-09 06:11:52.363 UTC",
   "is_from_sync_and_pending_installation": false,
   "is_generated_icon": false,
   "is_locally_installed": true,
   "is_storage_isolated": false,
   "is_uninstalling": false,
   "last_badging_time": "1970-01-12 14:48:29.918 UTC",
   "last_launch_time": "1970-01-02 16:03:30.110 UTC",
   "launch_handler": {
      "navigate_existing_client": "kAlways",
      "route_to": "kNewClient"
   },
   "launch_query_params": "3248422070",
   "manifest_id": null,
   "manifest_url": "https://example.com/manifest1234.json",
   "note_taking_new_note_url": "",
   "protocol_handlers": [ {
      "protocol": "web+test244307310",
      "url": "https://example.com/244307310"
   }, {
      "protocol": "web+test244307311",
      "url": "https://example.com/244307311"
   }, {
      "protocol": "web+test244307312",
      "url": "https://example.com/244307312"
   }, {
      "protocol": "web+test244307313",
      "url": "https://example.com/244307313"
   }, {
      "protocol": "web+test244307314",
      "url": "https://example.com/244307314"
   } ],
   "run_on_os_login_mode": "windowed",
   "scope": "https://example.com/scope1234/",
   "share_target": null,
   "shortcuts_menu_item_infos": [ {
      "icons": {
         "ANY": [  ],
         "MASKABLE": [ {
            "square_size_px": 30,
            "url": "https://example.com/shortcuts/icon290010843223"
         }, {
            "square_size_px": 15,
            "url": "https://example.com/shortcuts/icon290010843221"
         } ],
         "MONOCHROME": [ {
            "square_size_px": 23,
            "url": "https://example.com/shortcuts/icon290010843222"
         }, {
            "square_size_px": 8,
            "url": "https://example.com/shortcuts/icon290010843220"
         } ]
      },
      "name": "shortcut29001084322",
      "url": "https://example.com/scope1234/shortcut29001084322"
   }, {
      "icons": {
         "ANY": [ {
            "square_size_px": 4,
            "url": "https://example.com/shortcuts/icon290010843210"
         } ],
         "MASKABLE": [ {
            "square_size_px": 24,
            "url": "https://example.com/shortcuts/icon290010843212"
         }, {
            "square_size_px": 19,
            "url": "https://example.com/shortcuts/icon290010843211"
         } ],
         "MONOCHROME": [  ]
      },
      "name": "shortcut29001084321",
      "url": "https://example.com/scope1234/shortcut29001084321"
   }, {
      "icons": {
         "ANY": [ {
            "square_size_px": 0,
            "url": "https://example.com/shortcuts/icon290010843200"
         } ],
         "MASKABLE": [  ],
         "MONOCHROME": [ {
            "square_size_px": 23,
            "url": "https://example.com/shortcuts/icon290010843202"
         }, {
            "square_size_px": 16,
            "url": "https://example.com/shortcuts/icon290010843201"
         } ]
      },
      "name": "shortcut29001084320",
      "url": "https://example.com/scope1234/shortcut29001084320"
   } ],
   "sources": [ "WebAppStore", "Sync", "Default" ],
   "start_url": "https://example.com/scope1234/start1234",
   "sync_fallback_data": {
      "icon_infos": [ {
         "purpose": "ANY",
         "square_size_px": null,
         "url": "https://example.com/icon1783899413"
      }, {
         "purpose": "ANY",
         "square_size_px": null,
         "url": "https://example.com/icon3011162902"
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
      "origin": "https://app-9974471690.com",
      "paths": [  ]
   }, {
      "exclude_paths": [  ],
      "has_origin_wildcard": true,
      "origin": "https://app-9974471691.com",
      "paths": [  ]
   }, {
      "exclude_paths": [  ],
      "has_origin_wildcard": true,
      "origin": "https://app-9974471692.com",
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
