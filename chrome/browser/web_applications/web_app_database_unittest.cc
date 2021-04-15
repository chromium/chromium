// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_database.h"

#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/sync/model/model_type_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

class RandomHelper {
 public:
  explicit RandomHelper(const uint32_t seed)
      :  // Seed of 0 and 1 generate the same sequence, so skip 0.
        generator_(seed + 1),
        distribution_(0u, UINT32_MAX) {}

  uint32_t next_uint() { return distribution_(generator_); }

  // Return an unsigned int between 0 (inclusive) and bound (exclusive).
  uint32_t next_uint(uint32_t bound) { return next_uint() % bound; }

  bool next_bool() { return next_uint() & 1u; }

 private:
  std::default_random_engine generator_;
  std::uniform_int_distribution<uint32_t> distribution_;
};

}  // namespace

class WebAppDatabaseTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();
    test_registry_controller_->SetUp(profile());
  }

  static apps::FileHandlers CreateFileHandlers(uint32_t suffix) {
    apps::FileHandlers file_handlers;

    for (unsigned int i = 0; i < 5; ++i) {
      std::string suffix_str =
          base::NumberToString(suffix) + base::NumberToString(i);

      apps::FileHandler::AcceptEntry accept_entry1;
      accept_entry1.mime_type = "application/" + suffix_str + "+foo";
      accept_entry1.file_extensions.insert("." + suffix_str + "a");
      accept_entry1.file_extensions.insert("." + suffix_str + "b");

      apps::FileHandler::AcceptEntry accept_entry2;
      accept_entry2.mime_type = "application/" + suffix_str + "+bar";
      accept_entry2.file_extensions.insert("." + suffix_str + "a");
      accept_entry2.file_extensions.insert("." + suffix_str + "b");

      apps::FileHandler file_handler;
      file_handler.action = GURL("https://example.com/open-" + suffix_str);
      file_handler.accept.push_back(std::move(accept_entry1));
      file_handler.accept.push_back(std::move(accept_entry2));

      file_handlers.push_back(std::move(file_handler));
    }

    return file_handlers;
  }

  static apps::ShareTarget CreateShareTarget(uint32_t suffix) {
    apps::ShareTarget share_target;
    share_target.action =
        GURL("https://example.com/path/target/" + base::NumberToString(suffix));
    share_target.method = (suffix % 2 == 0) ? apps::ShareTarget::Method::kPost
                                            : apps::ShareTarget::Method::kGet;
    share_target.enctype = (suffix / 2 % 2 == 0)
                               ? apps::ShareTarget::Enctype::kMultipartFormData
                               : apps::ShareTarget::Enctype::kFormUrlEncoded;

    if (suffix % 3 != 0)
      share_target.params.title = "title" + base::NumberToString(suffix);
    if (suffix % 3 != 1)
      share_target.params.text = "text" + base::NumberToString(suffix);
    if (suffix % 3 != 2)
      share_target.params.url = "url" + base::NumberToString(suffix);

    for (uint32_t index = 0; index < suffix % 5; ++index) {
      apps::ShareTarget::Files files;
      files.name = "files" + base::NumberToString(index);
      files.accept.push_back(".extension" + base::NumberToString(index));
      files.accept.push_back("type/subtype" + base::NumberToString(index));
      share_target.params.files.push_back(files);
    }

    return share_target;
  }

  static std::vector<apps::ProtocolHandlerInfo> CreateProtocolHandlers(
      uint32_t suffix) {
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers;

    for (unsigned int i = 0; i < 5; ++i) {
      std::string suffix_str =
          base::NumberToString(suffix) + base::NumberToString(i);

      apps::ProtocolHandlerInfo protocol_handler;
      protocol_handler.protocol = "web+test" + suffix_str;
      protocol_handler.url = GURL("https://example.com/%s");

      protocol_handlers.push_back(std::move(protocol_handler));
    }

    return protocol_handlers;
  }

  static std::vector<apps::UrlHandlerInfo> CreateUrlHandlers(uint32_t suffix) {
    std::vector<apps::UrlHandlerInfo> url_handlers;

    for (unsigned int i = 0; i < 3; ++i) {
      std::string suffix_str =
          base::NumberToString(suffix) + base::NumberToString(i);

      apps::UrlHandlerInfo url_handler;
      url_handler.origin =
          url::Origin::Create(GURL("https://app-" + suffix_str + ".com/"));
      url_handler.has_origin_wildcard = true;
      url_handlers.push_back(std::move(url_handler));
    }

    return url_handlers;
  }

  static blink::mojom::CaptureLinks CreateCaptureLinks(uint32_t suffix) {
    return static_cast<blink::mojom::CaptureLinks>(
        suffix % static_cast<uint32_t>(blink::mojom::CaptureLinks::kMaxValue));
  }

  static std::vector<WebApplicationShortcutsMenuItemInfo>
  CreateShortcutsMenuItemInfos(const std::string& base_url,
                               RandomHelper& random) {
    const uint32_t suffix = random.next_uint();
    std::vector<WebApplicationShortcutsMenuItemInfo> shortcuts_menu_item_infos;
    for (int i = random.next_uint(4) + 1; i >= 0; --i) {
      std::string suffix_str =
          base::NumberToString(suffix) + base::NumberToString(i);
      WebApplicationShortcutsMenuItemInfo shortcut_info;
      shortcut_info.url = GURL(base_url + "/shortcut" + suffix_str);
      shortcut_info.name = base::UTF8ToUTF16("shortcut" + suffix_str);
      std::vector<WebApplicationShortcutsMenuItemInfo::Icon> shortcut_icons_any;
      std::vector<WebApplicationShortcutsMenuItemInfo::Icon>
          shortcut_icons_maskable;
      for (int j = random.next_uint(4) + 1; j >= 0; --j) {
        std::string icon_suffix_str = suffix_str + base::NumberToString(j);
        WebApplicationShortcutsMenuItemInfo::Icon shortcut_icon;
        shortcut_icon.url =
            GURL(base_url + "/shortcuts/icon" + icon_suffix_str);
        // Within each shortcut_icons_*, square_size_px must be unique.
        shortcut_icon.square_size_px = (j * 10) + random.next_uint(10);
        if (random.next_bool())
          shortcut_icons_any.push_back(std::move(shortcut_icon));
        else
          shortcut_icons_maskable.push_back(std::move(shortcut_icon));
      }
      shortcut_info.SetShortcutIconInfosForPurpose(
          IconPurpose::ANY, std::move(shortcut_icons_any));
      shortcut_info.SetShortcutIconInfosForPurpose(
          IconPurpose::MASKABLE, std::move(shortcut_icons_maskable));
      shortcuts_menu_item_infos.emplace_back(std::move(shortcut_info));
    }
    return shortcuts_menu_item_infos;
  }

  static std::vector<IconSizes> CreateDownloadedShortcutsMenuIconsSizes(
      RandomHelper& random) {
    std::vector<IconSizes> results;
    for (unsigned int i = 0; i < 3; ++i) {
      IconSizes result;
      std::vector<SquareSizePx> shortcuts_menu_icon_sizes_any;
      std::vector<SquareSizePx> shortcuts_menu_icon_sizes_maskable;
      for (unsigned int j = 0; j < i; ++j) {
        shortcuts_menu_icon_sizes_any.emplace_back(random.next_uint(256) + 1);
        shortcuts_menu_icon_sizes_maskable.emplace_back(random.next_uint(256) +
                                                        1);
      }
      result.SetSizesForPurpose(IconPurpose::ANY,
                                std::move(shortcuts_menu_icon_sizes_any));
      result.SetSizesForPurpose(IconPurpose::MASKABLE,
                                std::move(shortcuts_menu_icon_sizes_maskable));
      results.emplace_back(std::move(result));
    }
    return results;
  }

  static std::unique_ptr<WebApp> CreateWebApp(const std::string& base_url,
                                              const uint32_t seed) {
    RandomHelper random(seed);

    const std::string seed_str = base::NumberToString(seed);
    const auto start_url = base_url + seed_str;
    const AppId app_id = GenerateAppIdFromURL(GURL(start_url));
    const std::string name = "Name" + seed_str;
    const std::string description = "Description" + seed_str;
    const std::string scope = base_url + "/scope" + seed_str;
    const base::Optional<SkColor> theme_color = random.next_uint();
    const base::Optional<SkColor> background_color = random.next_uint();
    const base::Optional<SkColor> synced_theme_color = random.next_uint();
    auto app = std::make_unique<WebApp>(app_id);

    // Generate all possible permutations of field values in a random way:
    if (random.next_bool())
      app->AddSource(Source::kSystem);
    if (random.next_bool())
      app->AddSource(Source::kPolicy);
    if (random.next_bool())
      app->AddSource(Source::kWebAppStore);
    if (random.next_bool())
      app->AddSource(Source::kSync);
    if (random.next_bool())
      app->AddSource(Source::kDefault);
    // Must always be at least one source.
    if (!app->HasAnySources())
      app->AddSource(Source::kSync);

    app->SetName(name);
    app->SetDescription(description);
    app->SetStartUrl(GURL(start_url));
    app->SetScope(GURL(scope));
    app->SetThemeColor(theme_color);
    app->SetBackgroundColor(background_color);
    app->SetIsLocallyInstalled(random.next_bool());
    app->SetIsInSyncInstall(random.next_bool());
    app->SetUserDisplayMode(random.next_bool() ? DisplayMode::kBrowser
                                               : DisplayMode::kStandalone);

    const base::Time last_badging_time =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMilliseconds(random.next_uint());
    app->SetLastBadgingTime(last_badging_time);

    const base::Time last_launch_time =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMilliseconds(random.next_uint());
    app->SetLastLaunchTime(last_launch_time);

    const base::Time install_time =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMilliseconds(random.next_uint());
    app->SetInstallTime(install_time);

    const DisplayMode display_modes[4] = {
        DisplayMode::kBrowser, DisplayMode::kMinimalUi,
        DisplayMode::kStandalone, DisplayMode::kFullscreen};
    app->SetDisplayMode(display_modes[random.next_uint(4)]);

    // Add only unique display modes.
    std::set<DisplayMode> display_mode_override;
    int num_display_mode_override_tries = random.next_uint(5);
    for (int i = 0; i < num_display_mode_override_tries; i++)
      display_mode_override.insert(display_modes[random.next_uint(4)]);
    app->SetDisplayModeOverride(std::vector<DisplayMode>(
        display_mode_override.begin(), display_mode_override.end()));

    if (random.next_bool())
      app->SetLaunchQueryParams(base::NumberToString(random.next_uint()));

    const RunOnOsLoginMode run_on_os_login_modes[3] = {
        RunOnOsLoginMode::kNotRun, RunOnOsLoginMode::kWindowed,
        RunOnOsLoginMode::kMinimized};
    app->SetRunOnOsLoginMode(run_on_os_login_modes[random.next_uint(3)]);

    const SquareSizePx size = 256;
    const int num_icons = random.next_uint(10);
    std::vector<WebApplicationIconInfo> icon_infos(num_icons);
    for (int i = 0; i < num_icons; i++) {
      WebApplicationIconInfo icon;
      icon.url =
          GURL(base_url + "/icon" + base::NumberToString(random.next_uint()));
      if (random.next_bool())
        icon.square_size_px = size;

      int purpose = random.next_uint(3);
      if (purpose == 0)
        icon.purpose = blink::mojom::ManifestImageResource_Purpose::ANY;
      if (purpose == 1)
        icon.purpose = blink::mojom::ManifestImageResource_Purpose::MASKABLE;
      // if (purpose == 2), leave purpose unset. Should default to ANY.

      icon_infos[i] = icon;
    }
    app->SetIconInfos(icon_infos);
    if (random.next_bool())
      app->SetDownloadedIconSizes(IconPurpose::ANY, {size});
    if (random.next_bool())
      app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {size});
    app->SetIsGeneratedIcon(random.next_bool());

    app->SetFileHandlers(CreateFileHandlers(random.next_uint()));
    if (random.next_bool())
      app->SetShareTarget(CreateShareTarget(random.next_uint()));
    app->SetProtocolHandlers(CreateProtocolHandlers(random.next_uint()));
    app->SetUrlHandlers(CreateUrlHandlers(random.next_uint()));
    app->SetCaptureLinks(CreateCaptureLinks(random.next_uint()));

    const int num_additional_search_terms = random.next_uint(8);
    std::vector<std::string> additional_search_terms(
        num_additional_search_terms);
    for (int i = 0; i < num_additional_search_terms; ++i) {
      additional_search_terms[i] =
          "Foo_" + seed_str + "_" + base::NumberToString(i);
    }
    app->SetAdditionalSearchTerms(std::move(additional_search_terms));

    app->SetShortcutsMenuItemInfos(
        CreateShortcutsMenuItemInfos(base_url, random));
    app->SetDownloadedShortcutsMenuIconsSizes(
        CreateDownloadedShortcutsMenuIconsSizes(random));
    app->SetManifestUrl(GURL(base_url + "manifest" + seed_str + ".json"));

    if (IsChromeOs()) {
      auto chromeos_data = base::make_optional<WebAppChromeOsData>();
      chromeos_data->show_in_launcher = random.next_bool();
      chromeos_data->show_in_search = random.next_bool();
      chromeos_data->show_in_management = random.next_bool();
      chromeos_data->is_disabled = random.next_bool();
      chromeos_data->oem_installed = random.next_bool();
      app->SetWebAppChromeOsData(std::move(chromeos_data));
    }

    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = "Sync" + name;
    sync_fallback_data.theme_color = synced_theme_color;
    sync_fallback_data.scope = app->scope();
    sync_fallback_data.icon_infos = app->icon_infos();
    app->SetSyncFallbackData(std::move(sync_fallback_data));

    return app;
  }

  bool IsDatabaseRegistryEqualToRegistrar() {
    Registry registry = database_factory().ReadRegistry();
    return IsRegistryEqual(mutable_registrar().registry(), registry);
  }

  void WriteBatch(
      std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch) {
    base::RunLoop run_loop;

    database_factory().store()->CommitWriteBatch(
        std::move(write_batch),
        base::BindLambdaForTesting(
            [&](const base::Optional<syncer::ModelError>& error) {
              EXPECT_FALSE(error);
              run_loop.Quit();
            }));

    run_loop.Run();
  }

  Registry WriteWebApps(const std::string& base_url, int num_apps) {
    Registry registry;

    auto write_batch = database_factory().store()->CreateWriteBatch();

    for (int i = 0; i < num_apps; ++i) {
      auto app = CreateWebApp(base_url, i);
      auto proto = WebAppDatabase::CreateWebAppProto(*app);
      const auto app_id = app->app_id();

      write_batch->WriteData(app_id, proto->SerializeAsString());

      registry.emplace(app_id, std::move(app));
    }

    WriteBatch(std::move(write_batch));

    return registry;
  }

 protected:
  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }

  TestWebAppDatabaseFactory& database_factory() {
    return controller().database_factory();
  }

  WebAppRegistrar& registrar() { return controller().registrar(); }

  WebAppRegistrarMutable& mutable_registrar() {
    return controller().mutable_registrar();
  }

  WebAppSyncBridge& sync_bridge() { return controller().sync_bridge(); }

 private:
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
};

TEST_F(WebAppDatabaseTest, WriteAndReadRegistry) {
  controller().Init();
  EXPECT_TRUE(registrar().is_empty());

  const int num_apps = 20;
  const std::string base_url = "https://example.com/path";

  auto app = CreateWebApp(base_url, 0);
  auto app_id = app->app_id();
  controller().RegisterApp(std::move(app));
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  for (int i = 1; i <= num_apps; ++i) {
    auto extra_app = CreateWebApp(base_url, i);
    controller().RegisterApp(std::move(extra_app));
  }
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  controller().UnregisterApp(app_id);
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());

  controller().UnregisterAll();
  EXPECT_TRUE(IsDatabaseRegistryEqualToRegistrar());
}

TEST_F(WebAppDatabaseTest, WriteAndDeleteAppsWithCallbacks) {
  controller().Init();
  EXPECT_TRUE(registrar().is_empty());

  const int num_apps = 10;
  const std::string base_url = "https://example.com/path";

  RegistryUpdateData::Apps apps_to_create;
  std::vector<AppId> apps_to_delete;
  Registry expected_registry;

  for (int i = 0; i < num_apps; ++i) {
    std::unique_ptr<WebApp> app = CreateWebApp(base_url, i);
    apps_to_delete.push_back(app->app_id());
    apps_to_create.push_back(std::move(app));

    std::unique_ptr<WebApp> expected_app = CreateWebApp(base_url, i);
    expected_registry.emplace(expected_app->app_id(), std::move(expected_app));
  }

  {
    base::RunLoop run_loop;

    std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

    for (std::unique_ptr<WebApp>& web_app : apps_to_create)
      update->CreateApp(std::move(web_app));

    sync_bridge().CommitUpdate(std::move(update),
                               base::BindLambdaForTesting([&](bool success) {
                                 EXPECT_TRUE(success);
                                 run_loop.Quit();
                               }));
    run_loop.Run();

    Registry registry_written = database_factory().ReadRegistry();
    EXPECT_TRUE(IsRegistryEqual(registry_written, expected_registry));
  }

  {
    base::RunLoop run_loop;

    std::unique_ptr<WebAppRegistryUpdate> update = sync_bridge().BeginUpdate();

    for (const AppId& app_id : apps_to_delete)
      update->DeleteApp(app_id);

    sync_bridge().CommitUpdate(std::move(update),
                               base::BindLambdaForTesting([&](bool success) {
                                 EXPECT_TRUE(success);
                                 run_loop.Quit();
                               }));
    run_loop.Run();

    Registry registry_deleted = database_factory().ReadRegistry();
    EXPECT_TRUE(registry_deleted.empty());
  }
}

TEST_F(WebAppDatabaseTest, OpenDatabaseAndReadRegistry) {
  Registry registry = WriteWebApps("https://example.com/path", 20);

  controller().Init();
  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry));
}

TEST_F(WebAppDatabaseTest, BackwardCompatibility_WebAppWithOnlyRequiredFields) {
  const GURL start_url{"https://example.com/"};
  const AppId app_id = GenerateAppIdFromURL(start_url);
  const std::string name = "App Name";
  const auto user_display_mode = DisplayMode::kBrowser;
  const bool is_locally_installed = true;

  std::vector<std::unique_ptr<WebAppProto>> protos;

  // Create a proto with |required| only fields.
  // Do not add new fields in this test: any new fields should be |optional|.
  auto proto = std::make_unique<WebAppProto>();
  {
    sync_pb::WebAppSpecifics sync_proto;
    sync_proto.set_start_url(start_url.spec());
    sync_proto.set_user_display_mode(
        ToWebAppSpecificsUserDisplayMode(user_display_mode));
    *(proto->mutable_sync_data()) = std::move(sync_proto);
  }

  proto->set_name(name);
  proto->set_is_locally_installed(is_locally_installed);

  proto->mutable_sources()->set_system(false);
  proto->mutable_sources()->set_policy(false);
  proto->mutable_sources()->set_web_app_store(false);
  proto->mutable_sources()->set_sync(true);
  proto->mutable_sources()->set_default_(false);

  if (IsChromeOs()) {
    proto->mutable_chromeos_data()->set_show_in_launcher(false);
    proto->mutable_chromeos_data()->set_show_in_search(false);
    proto->mutable_chromeos_data()->set_show_in_management(false);
    proto->mutable_chromeos_data()->set_is_disabled(true);
  }

  protos.push_back(std::move(proto));
  database_factory().WriteProtos(protos);

  // Read the registry: the proto parsing may fail while reading the proto
  // above.
  controller().Init();

  const WebApp* app = registrar().GetAppById(app_id);
  EXPECT_EQ(app_id, app->app_id());
  EXPECT_EQ(start_url, app->start_url());
  EXPECT_EQ(name, app->name());
  EXPECT_EQ(user_display_mode, app->user_display_mode());
  EXPECT_EQ(is_locally_installed, app->is_locally_installed());
  EXPECT_TRUE(app->IsSynced());
  EXPECT_FALSE(app->IsDefaultApp());

  if (IsChromeOs()) {
    EXPECT_FALSE(app->chromeos_data()->show_in_launcher);
    EXPECT_FALSE(app->chromeos_data()->show_in_search);
    EXPECT_FALSE(app->chromeos_data()->show_in_management);
    EXPECT_TRUE(app->chromeos_data()->is_disabled);
  } else {
    EXPECT_FALSE(app->chromeos_data().has_value());
  }
}

TEST_F(WebAppDatabaseTest, WebAppWithoutOptionalFields) {
  controller().Init();

  const auto start_url = GURL("https://example.com/");
  const AppId app_id = GenerateAppIdFromURL(GURL(start_url));
  const std::string name = "Name";
  const auto user_display_mode = DisplayMode::kBrowser;

  auto app = std::make_unique<WebApp>(app_id);

  // Required fields:
  app->SetStartUrl(start_url);
  app->SetName(name);
  app->SetUserDisplayMode(user_display_mode);
  app->SetIsLocallyInstalled(false);
  // chromeos_data should always be set on ChromeOS.
  if (IsChromeOs())
    app->SetWebAppChromeOsData(base::make_optional<WebAppChromeOsData>());

  EXPECT_FALSE(app->HasAnySources());
  for (int i = Source::kMinValue; i <= Source::kMaxValue; ++i) {
    app->AddSource(static_cast<Source::Type>(i));
    EXPECT_TRUE(app->HasAnySources());
  }

  // Let optional fields be empty:
  EXPECT_EQ(app->display_mode(), DisplayMode::kUndefined);
  EXPECT_TRUE(app->display_mode_override().empty());
  EXPECT_TRUE(app->description().empty());
  EXPECT_TRUE(app->scope().is_empty());
  EXPECT_FALSE(app->theme_color().has_value());
  EXPECT_FALSE(app->background_color().has_value());
  EXPECT_TRUE(app->icon_infos().empty());
  EXPECT_TRUE(app->downloaded_icon_sizes(IconPurpose::ANY).empty());
  EXPECT_TRUE(app->downloaded_icon_sizes(IconPurpose::MASKABLE).empty());
  EXPECT_FALSE(app->is_generated_icon());
  EXPECT_FALSE(app->is_in_sync_install());
  EXPECT_TRUE(app->sync_fallback_data().name.empty());
  EXPECT_FALSE(app->sync_fallback_data().theme_color.has_value());
  EXPECT_FALSE(app->sync_fallback_data().scope.is_valid());
  EXPECT_TRUE(app->sync_fallback_data().icon_infos.empty());
  EXPECT_TRUE(app->file_handlers().empty());
  EXPECT_FALSE(app->share_target().has_value());
  EXPECT_TRUE(app->additional_search_terms().empty());
  EXPECT_TRUE(app->protocol_handlers().empty());
  EXPECT_TRUE(app->url_handlers().empty());
  EXPECT_TRUE(app->last_badging_time().is_null());
  EXPECT_TRUE(app->last_launch_time().is_null());
  EXPECT_TRUE(app->install_time().is_null());
  EXPECT_TRUE(app->shortcuts_menu_item_infos().empty());
  EXPECT_TRUE(app->downloaded_shortcuts_menu_icons_sizes().empty());
  EXPECT_EQ(app->run_on_os_login_mode(), RunOnOsLoginMode::kNotRun);
  EXPECT_TRUE(app->manifest_url().is_empty());
  EXPECT_FALSE(app->manifest_id().has_value());
  controller().RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_EQ(1UL, registry.size());

  std::unique_ptr<WebApp>& app_copy = registry.at(app_id);

  // Required fields were serialized:
  EXPECT_EQ(app_id, app_copy->app_id());
  EXPECT_EQ(start_url, app_copy->start_url());
  EXPECT_EQ(name, app_copy->name());
  EXPECT_EQ(user_display_mode, app_copy->user_display_mode());
  EXPECT_FALSE(app_copy->is_locally_installed());

  auto& chromeos_data = app_copy->chromeos_data();
  if (IsChromeOs()) {
    EXPECT_TRUE(chromeos_data->show_in_launcher);
    EXPECT_TRUE(chromeos_data->show_in_search);
    EXPECT_TRUE(chromeos_data->show_in_management);
    EXPECT_FALSE(chromeos_data->is_disabled);
    EXPECT_FALSE(chromeos_data->oem_installed);
  } else {
    EXPECT_FALSE(chromeos_data.has_value());
  }

  for (int i = Source::kMinValue; i <= Source::kMaxValue; ++i) {
    EXPECT_TRUE(app_copy->HasAnySources());
    app_copy->RemoveSource(static_cast<Source::Type>(i));
  }
  EXPECT_FALSE(app_copy->HasAnySources());

  // No optional fields.
  EXPECT_EQ(app_copy->display_mode(), DisplayMode::kUndefined);
  EXPECT_TRUE(app_copy->display_mode_override().empty());
  EXPECT_TRUE(app_copy->description().empty());
  EXPECT_TRUE(app_copy->scope().is_empty());
  EXPECT_FALSE(app_copy->theme_color().has_value());
  EXPECT_FALSE(app_copy->background_color().has_value());
  EXPECT_TRUE(app_copy->last_badging_time().is_null());
  EXPECT_TRUE(app_copy->last_launch_time().is_null());
  EXPECT_TRUE(app_copy->install_time().is_null());
  EXPECT_TRUE(app_copy->icon_infos().empty());
  EXPECT_TRUE(app_copy->downloaded_icon_sizes(IconPurpose::ANY).empty());
  EXPECT_TRUE(app_copy->downloaded_icon_sizes(IconPurpose::MASKABLE).empty());
  EXPECT_FALSE(app_copy->is_generated_icon());
  EXPECT_FALSE(app_copy->is_in_sync_install());
  EXPECT_TRUE(app_copy->sync_fallback_data().name.empty());
  EXPECT_FALSE(app_copy->sync_fallback_data().theme_color.has_value());
  EXPECT_FALSE(app_copy->sync_fallback_data().scope.is_valid());
  EXPECT_TRUE(app_copy->sync_fallback_data().icon_infos.empty());
  EXPECT_TRUE(app_copy->file_handlers().empty());
  EXPECT_FALSE(app_copy->share_target().has_value());
  EXPECT_TRUE(app_copy->additional_search_terms().empty());
  EXPECT_TRUE(app_copy->url_handlers().empty());
  EXPECT_TRUE(app_copy->shortcuts_menu_item_infos().empty());
  EXPECT_TRUE(app_copy->downloaded_shortcuts_menu_icons_sizes().empty());
  EXPECT_EQ(app_copy->run_on_os_login_mode(), RunOnOsLoginMode::kNotRun);
  EXPECT_TRUE(app_copy->manifest_url().is_empty());
  EXPECT_FALSE(app_copy->manifest_id().has_value());
}

TEST_F(WebAppDatabaseTest, WebAppWithManyIcons) {
  controller().Init();

  const int num_icons = 32;
  const std::string base_url = "https://example.com/path";

  auto app = CreateWebApp(base_url, 0);
  auto app_id = app->app_id();

  std::vector<WebApplicationIconInfo> icons;
  std::vector<SquareSizePx> sizes;
  for (int i = 1; i <= num_icons; ++i) {
    WebApplicationIconInfo icon;
    icon.url = GURL(base_url + "/icon" + base::NumberToString(num_icons));
    // Let size equals the icon's number squared.
    icon.square_size_px = i * i;
    sizes.push_back(*icon.square_size_px);
    icons.push_back(std::move(icon));
  }
  app->SetIconInfos(std::move(icons));
  app->SetDownloadedIconSizes(IconPurpose::ANY, std::move(sizes));
  app->SetIsGeneratedIcon(false);

  controller().RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_EQ(1UL, registry.size());

  std::unique_ptr<WebApp>& app_copy = registry.at(app_id);
  EXPECT_EQ(static_cast<unsigned>(num_icons), app_copy->icon_infos().size());
  for (int i = 1; i <= num_icons; ++i) {
    const int icon_size_in_px = i * i;
    EXPECT_EQ(icon_size_in_px, app_copy->icon_infos()[i - 1].square_size_px);
  }
  EXPECT_FALSE(app_copy->is_generated_icon());
}

TEST_F(WebAppDatabaseTest, WebAppWithFileHandlersRoundTrip) {
  controller().Init();

  const std::string base_url = "https://example.com/path";
  auto app = CreateWebApp(base_url, 0);
  auto app_id = app->app_id();

  apps::FileHandlers file_handlers;

  apps::FileHandler file_handler1;
  file_handler1.action = GURL("https://example.com/path/csv");
  apps::FileHandler::AcceptEntry accept_csv;
  accept_csv.mime_type = "text/csv";
  accept_csv.file_extensions.insert(".csv");
  accept_csv.file_extensions.insert(".txt");
  file_handler1.accept.push_back(std::move(accept_csv));
  file_handlers.push_back(std::move(file_handler1));

  apps::FileHandler file_handler2;
  file_handler2.action = GURL("https://example.com/path/svg");
  apps::FileHandler::AcceptEntry accept_xml;
  accept_xml.mime_type = "text/xml";
  accept_xml.file_extensions.insert(".xml");
  file_handler2.accept.push_back(std::move(accept_xml));
  apps::FileHandler::AcceptEntry accept_svg;
  accept_svg.mime_type = "text/xml+svg";
  accept_svg.file_extensions.insert(".svg");
  file_handler2.accept.push_back(std::move(accept_svg));
  file_handlers.push_back(std::move(file_handler2));

  app->SetFileHandlers(std::move(file_handlers));

  controller().RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry));
}

TEST_F(WebAppDatabaseTest, WebAppWithShareTargetRoundTrip) {
  controller().Init();

  const std::string base_url = "https://example.com/path";
  auto app = CreateWebApp(base_url, 0);
  auto app_id = app->app_id();

  apps::ShareTarget share_target;
  share_target.action = GURL("https://example.com/path/target");
  share_target.method = apps::ShareTarget::Method::kPost;
  share_target.enctype = apps::ShareTarget::Enctype::kMultipartFormData;
  share_target.params.title = "Title";
  share_target.params.text = "Text";
  share_target.params.url = "Url";
  app->SetShareTarget(std::move(share_target));

  controller().RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry));
}

TEST_F(WebAppDatabaseTest, WebAppWithCaptureLinksRoundTrip) {
  controller().Init();

  const std::string base_url = "https://example.com/path";
  auto app = CreateWebApp(base_url, 0);
  auto app_id = app->app_id();

  app->SetCaptureLinks(blink::mojom::CaptureLinks::kExistingClientNavigate);

  controller().RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry));
}

TEST_F(WebAppDatabaseTest, WebAppWithUrlHandlersRoundTrip) {
  controller().Init();

  const std::string base_url = "https://example.com/path";
  auto app = CreateWebApp(base_url, 0);
  auto app_id = app->app_id();

  app->SetUrlHandlers(CreateUrlHandlers(1));

  controller().RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WebAppDatabaseTest, WebAppOemInstalledRoundTrip) {
  controller().Init();

  const std::string base_url = "https://example.com/path";
  auto app = CreateWebApp(base_url, 0);

  auto chromeos_data = base::make_optional<WebAppChromeOsData>();
  chromeos_data->oem_installed = true;
  app->SetWebAppChromeOsData(std::move(chromeos_data));

  controller().RegisterApp(std::move(app));

  Registry registry = database_factory().ReadRegistry();
  EXPECT_TRUE(IsRegistryEqual(mutable_registrar().registry(), registry));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace web_app
