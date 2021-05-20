// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

std::string WebAppToPlatformAgnosticString(std::unique_ptr<WebApp> web_app) {
  // Force this to be nullopt to avoid platform specific differences.
  web_app->SetWebAppChromeOsData(absl::nullopt);
  std::stringstream ss;
  ss << *web_app;
  return ss.str();
}

}  // namespace

TEST(WebAppTest, HasAnySources) {
  WebApp app{GenerateAppIdFromURL(GURL("https://example.com"))};

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
  WebApp app{GenerateAppIdFromURL(GURL("https://example.com"))};

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
  WebApp app{GenerateAppIdFromURL(GURL("https://example.com"))};

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
  WebApp app{GenerateAppIdFromURL(GURL("https://example.com"))};

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

TEST(WebAppTest, EmptyAppToDebugString) {
  std::string debug_string =
      WebAppToPlatformAgnosticString(std::make_unique<WebApp>("empty_app"));
  EXPECT_EQ(debug_string,
            R"(app_id: empty_app
manifest_url: 
manifest_id: nullopt
name: 
start_url: 
launch_query_params: nullopt
scope: 
theme_color: none
background_color: none
display_mode: 
display_override:
user_display_mode: 
user_page_ordinal: INVALID[]
user_launch_ordinal: INVALID[]
sources:
is_locally_installed: 1
is_in_sync_install: 0
sync_fallback_data:
  name: 
  theme_color: none
  scope: 
  icon_infos:
description: 
last_badging_time: 1601-01-01 00:00:00.000 UTC
last_launch_time: 1601-01-01 00:00:00.000 UTC
install_time: 1601-01-01 00:00:00.000 UTC
is_generated_icon: 0
run_on_os_login_mode: not run
icon_infos:
downloaded_icon_sizes_any:
downloaded_icon_sizes_monochrome:
downloaded_icon_sizes_maskable:
shortcuts_menu_item_infos:
downloaded_shortcuts_menu_icons_sizes:
file_handlers:
file_handler_permission_blocked:0
share_target:
  nullopt
additional_search_terms:
protocol_handlers:
note_taking_new_note_url: 
url_handlers:
capture_links: kUndefined
chromeos_data:
  nullopt
system_web_app:
  nullopt
)") << "Copypastable expectation: \n"
    << debug_string;
}

TEST(WebAppTest, SampleAppToDebugString) {
  std::string debug_string = WebAppToPlatformAgnosticString(
      test::CreateRandomWebApp(GURL("https://example.com/"), /*seed=*/1234));
  EXPECT_EQ(debug_string,
            R"(app_id: eajjdjobhihlgobdfaehiiheinneagde
manifest_url: https://example.com/manifest1234.json
manifest_id: nullopt
name: Name1234
start_url: https://example.com/scope1234/start1234
launch_query_params: 3248422070
scope: https://example.com/scope1234/
theme_color: rgba(151,34,83,0.8823529411764706)
background_color: rgba(77,188,194,0.9686274509803922)
display_mode: fullscreen
display_override:
user_display_mode: standalone
user_page_ordinal: INVALID[]
user_launch_ordinal: INVALID[]
sources: WebAppStore Sync Default
is_locally_installed: 1
is_in_sync_install: 0
sync_fallback_data:
  name: SyncName1234
  theme_color: rgba(61,127,69,0.8431372549019608)
  scope: https://example.com/scope1234/
  icon_infos:
    url: https://example.com/icon1783899413
      square_size_px: none
      purpose: ANY
    url: https://example.com/icon3011162902
      square_size_px: none
      purpose: ANY
description: Description1234
last_badging_time: 1970-01-12 14:48:29.918 UTC
last_launch_time: 1970-01-02 16:03:30.110 UTC
install_time: 1970-01-09 06:11:52.363 UTC
is_generated_icon: 0
run_on_os_login_mode: minimized
icon_infos:
  url: https://example.com/icon1783899413
    square_size_px: none
    purpose: ANY
  url: https://example.com/icon3011162902
    square_size_px: none
    purpose: ANY
downloaded_icon_sizes_any: 256
downloaded_icon_sizes_monochrome: 256
downloaded_icon_sizes_maskable:
shortcuts_menu_item_infos:
  name: shortcut24741963851
    url: https://example.com/scope1234/shortcut24741963851
    icons:
      any:
      maskable:
        url: https://example.com/shortcuts/icon247419638512
        square_size_px: 28
      monochrome:
        url: https://example.com/shortcuts/icon247419638511
        square_size_px: 11
        url: https://example.com/shortcuts/icon247419638510
        square_size_px: 1
  name: shortcut24741963850
    url: https://example.com/scope1234/shortcut24741963850
    icons:
      any:
      maskable:
        url: https://example.com/shortcuts/icon247419638500
        square_size_px: 9
      monochrome:
        url: https://example.com/shortcuts/icon247419638501
        square_size_px: 18
downloaded_shortcuts_menu_icons_sizes:
  index: 0:
    any:
    maskable:
  index: 1:
    any: 58
    maskable: 160
  index: 2:
    any: 113 90
    maskable: 77 109
file_handlers:
  action: https://example.com/open-13087720410
    accept:
      mime_type: application/13087720410+foo
      file_extensions: .13087720410a .13087720410b
    accept:
      mime_type: application/13087720410+bar
      file_extensions: .13087720410a .13087720410b
  action: https://example.com/open-13087720411
    accept:
      mime_type: application/13087720411+foo
      file_extensions: .13087720411a .13087720411b
    accept:
      mime_type: application/13087720411+bar
      file_extensions: .13087720411a .13087720411b
  action: https://example.com/open-13087720412
    accept:
      mime_type: application/13087720412+foo
      file_extensions: .13087720412a .13087720412b
    accept:
      mime_type: application/13087720412+bar
      file_extensions: .13087720412a .13087720412b
  action: https://example.com/open-13087720413
    accept:
      mime_type: application/13087720413+foo
      file_extensions: .13087720413a .13087720413b
    accept:
      mime_type: application/13087720413+bar
      file_extensions: .13087720413a .13087720413b
  action: https://example.com/open-13087720414
    accept:
      mime_type: application/13087720414+foo
      file_extensions: .13087720414a .13087720414b
    accept:
      mime_type: application/13087720414+bar
      file_extensions: .13087720414a .13087720414b
file_handler_permission_blocked:0
share_target:
  nullopt
additional_search_terms:
  Foo_1234_0
  Foo_1234_1
  Foo_1234_2
  Foo_1234_3
protocol_handlers:
  protocol: web+test244307310 url: https://example.com/244307310
  protocol: web+test244307311 url: https://example.com/244307311
  protocol: web+test244307312 url: https://example.com/244307312
  protocol: web+test244307313 url: https://example.com/244307313
  protocol: web+test244307314 url: https://example.com/244307314
note_taking_new_note_url: 
url_handlers:
  origin: https://app-9974471690.com
    has_origin_wildcard: true
    paths:
    exclude_paths:
  origin: https://app-9974471691.com
    has_origin_wildcard: true
    paths:
    exclude_paths:
  origin: https://app-9974471692.com
    has_origin_wildcard: true
    paths:
    exclude_paths:
capture_links: kNewClient
chromeos_data:
  nullopt
system_web_app:
  nullopt
)") << "Copypastable expectation: \n"
    << debug_string;
}

}  // namespace web_app
