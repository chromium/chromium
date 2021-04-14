// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

std::string WebAppToPlatformAgnosticString(std::unique_ptr<WebApp> web_app) {
  // Force this to be nullopt to avoid platform specific differences.
  web_app->SetWebAppChromeOsData(base::nullopt);
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

TEST(WebAppTest, CanUserUninstallExternalApp) {
  WebApp app{GenerateAppIdFromURL(GURL("https://example.com"))};

  app.AddSource(Source::kDefault);
  EXPECT_TRUE(app.IsPreinstalledApp());
  EXPECT_TRUE(app.CanUserUninstallExternalApp());

  app.AddSource(Source::kSync);
  EXPECT_TRUE(app.CanUserUninstallExternalApp());
  app.AddSource(Source::kWebAppStore);
  EXPECT_TRUE(app.CanUserUninstallExternalApp());

  app.AddSource(Source::kPolicy);
  EXPECT_FALSE(app.CanUserUninstallExternalApp());
  app.AddSource(Source::kSystem);
  EXPECT_FALSE(app.CanUserUninstallExternalApp());

  app.RemoveSource(Source::kSync);
  EXPECT_FALSE(app.CanUserUninstallExternalApp());
  app.RemoveSource(Source::kWebAppStore);
  EXPECT_FALSE(app.CanUserUninstallExternalApp());

  app.RemoveSource(Source::kSystem);
  EXPECT_FALSE(app.CanUserUninstallExternalApp());

  app.RemoveSource(Source::kPolicy);
  EXPECT_TRUE(app.CanUserUninstallExternalApp());

  EXPECT_TRUE(app.IsPreinstalledApp());
  app.RemoveSource(Source::kDefault);
  EXPECT_FALSE(app.IsPreinstalledApp());
}

TEST(WebAppTest, EmptyAppToDebugString) {
  std::string debug_string =
      WebAppToPlatformAgnosticString(std::make_unique<WebApp>("empty_app"));
  EXPECT_EQ(debug_string,
            R"(app_id: empty_app
  name: 
  start_url: 
  launch_query_params: nullopt
  scope: 
  theme_color: none
  background_color: none
  display_mode: 
  display_override: 0
  user_display_mode: 
  user_page_ordinal: INVALID[]
  user_launch_ordinal: INVALID[]
  sources: 
  is_locally_installed: 1
  is_in_sync_install: 0
  sync_fallback_data: 
    theme_color: none
    name: 
    scope: 
  description: 
  last_badging_time: 1601-01-01 00:00:00.000 UTC
  last_launch_time: 1601-01-01 00:00:00.000 UTC
  install_time: 1601-01-01 00:00:00.000 UTC
  is_generated_icon: 0
  run_on_os_login_mode: not run
  shortcuts_menu_item_infos_: 
  note_taking_new_note_url: 
  capture_links: kUndefined
  chromeos_data: nullopt
  system_web_app: nullopt
  manifest_url: 
  manifest_id: nullopt
)") << "Copypastable expectation: \n"
    << debug_string;
}

TEST(WebAppTest, SampleAppToDebugString) {
  std::string debug_string = WebAppToPlatformAgnosticString(
      test::CreateRandomWebApp("https://example.com/", /*seed=*/1234));
  EXPECT_EQ(debug_string,
            R"(app_id: omfgndaololjebhmknofljogcpffijdi
  name: Name1234
  start_url: https://example.com/1234
  launch_query_params: 3248422070
  scope: https://example.com//scope1234
  theme_color: rgba(220,247,174,0.6470588235294118)
  background_color: rgba(151,34,83,0.8823529411764706)
  display_mode: fullscreen
  display_override: 1
    browser
  user_display_mode: standalone
  user_page_ordinal: INVALID[]
  user_launch_ordinal: INVALID[]
  sources: System Sync Default 
  is_locally_installed: 1
  is_in_sync_install: 1
  sync_fallback_data: 
    theme_color: rgba(77,188,194,0.9686274509803922)
    name: SyncName1234
    scope: https://example.com//scope1234
    icon_info: url: https://example.com//icon1783899413 square_size_px: none purpose: ANY
    icon_info: url: https://example.com//icon3011162902 square_size_px: none purpose: ANY
  description: Description1234
  last_badging_time: 1970-01-15 00:09:41.850 UTC
  last_launch_time: 1970-01-12 14:48:29.918 UTC
  install_time: 1970-01-02 16:03:30.110 UTC
  is_generated_icon: 1
  run_on_os_login_mode: minimized
  icon_info: url: https://example.com//icon1783899413 square_size_px: none purpose: ANY
  icon_info: url: https://example.com//icon3011162902 square_size_px: none purpose: ANY
  downloaded_icon_sizes_any_: 256
  shortcuts_menu_item_infos_: 
    name: shortcut24741963851
    url: https://example.com//shortcut24741963851
    any:
      icon url: https://example.com//shortcuts/icon247419638511
      icon square_size_px11
      icon url: https://example.com//shortcuts/icon247419638510
      icon square_size_px1
    maskable:
      icon url: https://example.com//shortcuts/icon247419638512
      icon square_size_px28
    name: shortcut24741963850
    url: https://example.com//shortcut24741963850
    any:
      icon url: https://example.com//shortcuts/icon247419638501
      icon square_size_px18
    maskable:
      icon url: https://example.com//shortcuts/icon247419638500
      icon square_size_px9
  downloaded_shortcuts_menu_icons_sizes_.any:
  downloaded_shortcuts_menu_icons_sizes_.maskable:
  downloaded_shortcuts_menu_icons_sizes_.any: 58
  downloaded_shortcuts_menu_icons_sizes_.maskable: 160
  downloaded_shortcuts_menu_icons_sizes_.any: 232 77
  downloaded_shortcuts_menu_icons_sizes_.maskable: 113 154
  file_handler: action: https://example.com/open-33849121400 accept: mime_type: application/33849121400+foo file_extensions: .33849121400a .33849121400b accept: mime_type: application/33849121400+bar file_extensions: .33849121400a .33849121400b
  file_handler: action: https://example.com/open-33849121401 accept: mime_type: application/33849121401+foo file_extensions: .33849121401a .33849121401b accept: mime_type: application/33849121401+bar file_extensions: .33849121401a .33849121401b
  file_handler: action: https://example.com/open-33849121402 accept: mime_type: application/33849121402+foo file_extensions: .33849121402a .33849121402b accept: mime_type: application/33849121402+bar file_extensions: .33849121402a .33849121402b
  file_handler: action: https://example.com/open-33849121403 accept: mime_type: application/33849121403+foo file_extensions: .33849121403a .33849121403b accept: mime_type: application/33849121403+bar file_extensions: .33849121403a .33849121403b
  file_handler: action: https://example.com/open-33849121404 accept: mime_type: application/33849121404+foo file_extensions: .33849121404a .33849121404b accept: mime_type: application/33849121404+bar file_extensions: .33849121404a .33849121404b
  share_target: action: https://example.com/path/target/1210958276
method: POST
enctype: multipart/form-data
title: title1210958276
text: text1210958276
url: 
name: files0
accept: .extension0
accept: type/subtype0

  additional_search_term: Foo_1234_0
  additional_search_term: Foo_1234_1
  additional_search_term: Foo_1234_2
  additional_search_term: Foo_1234_3
  protocol_handler: protocol: web+test244307310 url: https://example.com/%s
  protocol_handler: protocol: web+test244307311 url: https://example.com/%s
  protocol_handler: protocol: web+test244307312 url: https://example.com/%s
  protocol_handler: protocol: web+test244307313 url: https://example.com/%s
  protocol_handler: protocol: web+test244307314 url: https://example.com/%s
  note_taking_new_note_url: 
  url_handler: origin: https://app-9974471690.comhas_origin_wildcard: truepaths: exclude_paths: 
  url_handler: origin: https://app-9974471691.comhas_origin_wildcard: truepaths: exclude_paths: 
  url_handler: origin: https://app-9974471692.comhas_origin_wildcard: truepaths: exclude_paths: 
  capture_links: kNewClient
  chromeos_data: nullopt
  system_web_app: nullopt
  manifest_url: https://example.com/manifest1234.json
  manifest_id: nullopt
)") << "Copypastable expectation: \n"
    << debug_string;
}

}  // namespace web_app
