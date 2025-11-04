// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/file_handling_sub_manager.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

struct FileHandlerMapping {
  std::string file_extension;
  std::string policy_id;
};

class FileHandlingSubManagerConfigureTest : public WebAppTest {
 public:
  using InstallResults = std::map<GURL /*install_url*/,
                                  ExternallyManagedAppManager::InstallResult>;
  using UninstallResults =
      std::map<GURL /*install_url*/, webapps::UninstallResultCode>;
  using SynchronizeFuture =
      base::test::TestFuture<InstallResults, UninstallResults>;

  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

  FileHandlingSubManagerConfigureTest() = default;
  ~FileHandlingSubManagerConfigureTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = FakeWebAppProvider::Get(profile());
    provider_->UseRealOsIntegrationManager();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  webapps::AppId InstallWebAppWithFileHandlers(
      apps::FileHandlers file_handlers) {
    std::unique_ptr<WebAppInstallInfo> info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kWebAppUrl);
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    info->file_handlers = file_handlers;
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;
    // InstallFromInfoWithParams is used instead of InstallFromInfo, because
    // InstallFromInfo doesn't register OS integration.
    provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return webapps::AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<webapps::AppId>();
  }

#if BUILDFLAG(IS_CHROMEOS)
  void UpdateDefaultHandlersPrefs(
      const std::vector<FileHandlerMapping>& handlers = {}) {
    base::Value::Dict pref_dict;
    for (const auto& handler : handlers) {
      pref_dict.Set(handler.file_extension, handler.policy_id);
    }
    profile()->GetTestingPrefService()->SetDict(
        prefs::kDefaultHandlersForFileExtensions, std::move(pref_dict));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

 protected:
  WebAppProvider& provider() { return *provider_; }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
};

TEST_F(FileHandlingSubManagerConfigureTest, InstallWithFilehandlers) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foobar";
      accept_entry.file_extensions.insert(".foobar");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-bar");
    file_handler.display_name = u"Bar opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/bar";
      accept_entry.file_extensions.insert(".bar");
      accept_entry.file_extensions.insert(".baz");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::os_state::WebAppOsIntegration& os_integration_state =
      state.value();
  ASSERT_TRUE(os_integration_state.has_file_handling());
  auto file_handling = os_integration_state.file_handling();

  ASSERT_EQ(file_handling.file_handlers_size(), 2);

  EXPECT_EQ(file_handling.file_handlers(0).accept_size(), 2);
  EXPECT_EQ(file_handling.file_handlers(0).display_name(), "Foo opener");
  EXPECT_EQ(file_handling.file_handlers(0).action(),
            "https://app.site/open-foo");
  EXPECT_EQ(file_handling.file_handlers(0).accept(0).mimetype(),
            "application/foo");
  EXPECT_EQ(file_handling.file_handlers(0).accept(0).file_extensions_size(), 1);
  EXPECT_EQ(file_handling.file_handlers(0).accept(0).file_extensions(0),
            ".foo");
  EXPECT_EQ(file_handling.file_handlers(0).accept(1).mimetype(),
            "application/foobar");
  EXPECT_EQ(file_handling.file_handlers(0).accept(1).file_extensions_size(), 1);
  EXPECT_EQ(file_handling.file_handlers(0).accept(1).file_extensions(0),
            ".foobar");

  EXPECT_EQ(file_handling.file_handlers(1).accept_size(), 1);
  EXPECT_EQ(file_handling.file_handlers(1).display_name(), "Bar opener");
  EXPECT_EQ(file_handling.file_handlers(1).action(),
            "https://app.site/open-bar");
  EXPECT_EQ(file_handling.file_handlers(1).accept(0).mimetype(),
            "application/bar");
  EXPECT_EQ(file_handling.file_handlers(1).accept(0).file_extensions_size(), 2);
  EXPECT_EQ(file_handling.file_handlers(1).accept(0).file_extensions(0),
            ".bar");
  EXPECT_EQ(file_handling.file_handlers(1).accept(0).file_extensions(1),
            ".baz");
}

TEST_F(FileHandlingSubManagerConfigureTest, UpdateUserChoiceDisallowed) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  base::test::TestFuture<void> future;
  provider().scheduler().PersistFileHandlersUserChoice(
      app_id, /*allowed=*/false, future.GetCallback());

  ASSERT_TRUE(future.Wait());

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::os_state::WebAppOsIntegration& os_integration_state =
      state.value();
  ASSERT_FALSE(os_integration_state.has_file_handling());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(FileHandlingSubManagerConfigureTest,
       PolicyDefinedFileHandlerOverUserChoice) {
  const GURL kInstallUrl =
      GURL("https://www.example.com/path/install_url.html");
  const GURL kStartUrl = GURL("https://example.com/path/");
  const GURL kManifestUrl = GURL("https://www.example.com/path/manifest.json");
  const webapps::AppId app_id = web_app::GenerateAppId(std::nullopt, kStartUrl);
  const std::string file_extension = ".txt";

  {
    fake_provider().GetFakeWebContentsManager()->CreateBasicInstallPageState(
        kInstallUrl, kManifestUrl, kWebAppUrl);
    auto& page_state =
        fake_provider().GetFakeWebContentsManager()->GetOrCreatePageState(
            kInstallUrl);

    auto manifest = blink::mojom::Manifest::New();
    manifest->id = manifest->start_url.GetWithoutRef();
    manifest->scope = manifest->start_url.GetWithoutFilename();
    manifest->manifest_url = page_state.manifest_url;
    manifest->start_url = kStartUrl;

    auto handler = blink::mojom::ManifestFileHandler::New();
    handler->action = kStartUrl;
    handler->name = u"Text";
    std::vector<std::u16string> extensions = {u".txt"};
    handler->accept.emplace(u"text/plain", extensions);
    manifest->file_handlers.push_back(std::move(handler));

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  // Install app with file handlers
  SynchronizeFuture result;
  std::vector<ExternalInstallOptions> install_options_list;
  install_options_list.emplace_back(kInstallUrl,
                                    /*user_display_mode=*/std::nullopt,
                                    ExternalInstallSource::kExternalPolicy);

  provider().externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kExternalPolicy,
      result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // Verify file handler os registration and default approval state.
  ASSERT_TRUE(provider().registrar_unsafe().GetAppById(app_id));
  auto initial_state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(initial_state.has_value());
  ASSERT_TRUE(initial_state.value().has_file_handling());
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(app_id),
      ApiApprovalState::kRequiresPrompt);

  // User disallows file handling for the app.
  base::test::TestFuture<void> future;
  provider().scheduler().PersistFileHandlersUserChoice(
      app_id, /*allowed=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Verify that the file handlers are in disallowed state.
  {
    EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(
                  app_id),
              ApiApprovalState::kDisallowed);
    EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerApprovalState(
                  app_id, file_extension),
              ApiApprovalState::kDisallowed);
    auto state_after_user_choice =
        provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
    ASSERT_TRUE(state_after_user_choice.has_value());
  }

  UpdateDefaultHandlersPrefs({{file_extension, kInstallUrl.spec()}});
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Verify that file handling is allowed.
  {
    auto final_state =
        provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
    ASSERT_TRUE(final_state.has_value());
    const proto::os_state::WebAppOsIntegration& os_integration_state =
        final_state.value();
    ASSERT_TRUE(os_integration_state.has_file_handling());

    // Policy takes precedence, so the effective state is now allowed.
    EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerApprovalState(
                  app_id, file_extension),
              ApiApprovalState::kAllowed);

    EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(
                  app_id),
              ApiApprovalState::kDisallowed);
  }
}

TEST_F(FileHandlingSubManagerConfigureTest,
       PolicyRemovedMidSessionRevertsToUserChoice) {
  const GURL kInstallUrl =
      GURL("https://www.example.com/path/install_url.html");
  const GURL kStartUrl = GURL("https://example.com/path/");
  const GURL kManifestUrl = GURL("https://www.example.com/path/manifest.json");
  const webapps::AppId app_id = web_app::GenerateAppId(std::nullopt, kStartUrl);
  const std::string file_extension = ".txt";

  {
    fake_provider().GetFakeWebContentsManager()->CreateBasicInstallPageState(
        kInstallUrl, kManifestUrl, kWebAppUrl);
    auto& page_state =
        fake_provider().GetFakeWebContentsManager()->GetOrCreatePageState(
            kInstallUrl);

    auto manifest = blink::mojom::Manifest::New();
    manifest->id = manifest->start_url.GetWithoutRef();
    manifest->scope = manifest->start_url.GetWithoutFilename();
    manifest->manifest_url = page_state.manifest_url;
    manifest->start_url = kStartUrl;

    auto handler = blink::mojom::ManifestFileHandler::New();
    handler->action = kStartUrl;
    handler->name = u"Text";
    std::vector<std::u16string> extensions = {u".txt"};
    handler->accept.emplace(u"text/plain", extensions);
    manifest->file_handlers.push_back(std::move(handler));

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  // Install app with file handlers
  SynchronizeFuture result;
  std::vector<ExternalInstallOptions> install_options_list;
  install_options_list.emplace_back(kInstallUrl,
                                    /*user_display_mode=*/std::nullopt,
                                    ExternalInstallSource::kExternalPolicy);

  provider().externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kExternalPolicy,
      result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // Verify file handler os registration and default approval state.
  ASSERT_TRUE(provider().registrar_unsafe().GetAppById(app_id));
  auto initial_state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(initial_state.has_value());
  ASSERT_TRUE(initial_state.value().has_file_handling());
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(app_id),
      ApiApprovalState::kRequiresPrompt);

  // User disallows file handling for the app.
  base::test::TestFuture<void> future;
  provider().scheduler().PersistFileHandlersUserChoice(
      app_id, /*allowed=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Setting pref to overwrite the user choice.
  UpdateDefaultHandlersPrefs({{file_extension, kInstallUrl.spec()}});
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  {
    ASSERT_EQ(provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(
                  app_id),
              ApiApprovalState::kDisallowed);
    ASSERT_EQ(provider().registrar_unsafe().GetAppFileHandlerApprovalState(
                  app_id, file_extension),
              ApiApprovalState::kAllowed);
    auto state_after_user_choice =
        provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
    ASSERT_TRUE(state_after_user_choice.has_value());
  }

  // Resetting pref value, file handler approval state should default to
  // previous user choice.
  UpdateDefaultHandlersPrefs();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  {
    EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(
                  app_id),
              ApiApprovalState::kDisallowed);
    EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerApprovalState(
                  app_id, file_extension),
              ApiApprovalState::kDisallowed);
  }
}

TEST_F(FileHandlingSubManagerConfigureTest,
       PolicyDefinedFileHandlerAddedMidSessionToManifest) {
  const GURL kInstallUrl("https://www.example.com/install.html");
  const GURL kStartUrl("https://example.com/path/");
  const GURL kManifestUrl("https://www.example.com/manifest.json");
  const webapps::AppId app_id = web_app::GenerateAppId(std::nullopt, kStartUrl);
  const std::string file_extension = ".txt";

  // Install the app WITHOUT any file handlers.
  {
    auto& page_state =
        fake_provider().GetFakeWebContentsManager()->GetOrCreatePageState(
            kInstallUrl);
    page_state.url_load_result = {webapps::WebAppUrlLoaderResult::kUrlLoaded};
    page_state.manifest_url = kManifestUrl;

    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = kStartUrl;
    manifest->scope = kStartUrl.GetWithoutFilename();
    page_state.manifest_before_default_processing = std::move(manifest);

    std::vector<ExternalInstallOptions> install_options_list;
    install_options_list.emplace_back(kInstallUrl,
                                      /*user_display_mode=*/std::nullopt,
                                      ExternalInstallSource::kExternalPolicy);
    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        std::move(install_options_list), ExternalInstallSource::kExternalPolicy,
        result.GetCallback());
    ASSERT_TRUE(result.Wait());
  }

  // User disallows file handling for the app.
  base::test::TestFuture<void> future;
  provider().scheduler().PersistFileHandlersUserChoice(
      app_id, /*allowed=*/false, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Modify the manifest to ADD a file handler.
  {
    auto& page_state =
        fake_provider().GetFakeWebContentsManager()->GetOrCreatePageState(
            kInstallUrl);

    auto handler = blink::mojom::ManifestFileHandler::New();
    handler->action = kStartUrl;
    handler->name = u"Text";
    handler->accept.emplace(u"text/plain",
                            std::vector<std::u16string>{u".txt"});
    page_state.manifest_before_default_processing->file_handlers.push_back(
        std::move(handler));
  }

  EXPECT_EQ(
      provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(app_id),
      ApiApprovalState::kDisallowed);
  EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerApprovalState(
                app_id, file_extension),
            ApiApprovalState::kDisallowed);

  // Allow the file handler via policy.
  UpdateDefaultHandlersPrefs({{file_extension, kInstallUrl.spec()}});
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_EQ(
      provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(app_id),
      ApiApprovalState::kDisallowed);
  EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerApprovalState(
                app_id, file_extension),
            ApiApprovalState::kAllowed);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(FileHandlingSubManagerConfigureTest, Uninstall) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  test::UninstallAllWebApps(profile());
  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_FALSE(state.has_value());
}

class FileHandlingSubManagerConfigureAndExecuteTest
    : public FileHandlingSubManagerConfigureTest {
 public:
  bool IsFileHandlingEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
    return false;
#else
    return true;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  WebAppFileHandlerManager& file_handler_manager() {
    return provider().os_integration_manager().file_handler_manager();
  }
};

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest, InstallWithFilehandlers) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foobar";
      accept_entry.file_extensions.insert(".foobar");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-bar");
    file_handler.display_name = u"Bar opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/bar";
      accept_entry.file_extensions.insert(".bar");
      accept_entry.file_extensions.insert(".baz");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::os_state::WebAppOsIntegration& os_integration_state =
      state.value();
  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    ASSERT_EQ(
        IsFileHandlingEnabled(),
        fake_os_integration().IsFileExtensionHandled(
            profile(), app_id,
            provider().registrar_unsafe().GetAppShortName(app_id), extension));
  }
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest,
       UpdateUserChoiceDisallowed) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);
  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::os_state::WebAppOsIntegration& os_integration_state =
      state.value();

  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    ASSERT_EQ(
        IsFileHandlingEnabled(),
        fake_os_integration().IsFileExtensionHandled(
            profile(), app_id,
            provider().registrar_unsafe().GetAppShortName(app_id), extension));
  }

  base::test::TestFuture<void> future;
  provider().scheduler().PersistFileHandlersUserChoice(
      app_id, /*allowed=*/false, future.GetCallback());

  ASSERT_TRUE(future.Wait());

  auto new_state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::os_state::WebAppOsIntegration& new_os_integration_state =
      new_state.value();
  ASSERT_FALSE(new_os_integration_state.has_file_handling());

  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    ASSERT_FALSE(fake_os_integration().IsFileExtensionHandled(
        profile(), app_id,
        provider().registrar_unsafe().GetAppShortName(app_id), extension));
  }
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest, Uninstall) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  proto::os_state::WebAppOsIntegration os_integration_state = state.value();
  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    ASSERT_EQ(
        IsFileHandlingEnabled(),
        fake_os_integration().IsFileExtensionHandled(
            profile(), app_id,
            provider().registrar_unsafe().GetAppShortName(app_id), extension));
  }
  test::UninstallAllWebApps(profile());
  auto new_state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_FALSE(new_state.has_value());
  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    EXPECT_FALSE(fake_os_integration().IsFileExtensionHandled(
        profile(), app_id,
        provider().registrar_unsafe().GetAppShortName(app_id), extension));
  }
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest,
       ForceUnregisterAppInRegistry) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);
  const std::string& app_name =
      provider().registrar_unsafe().GetAppShortName(app_id);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::os_state::WebAppOsIntegration& os_integration_state =
      state.value();
  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    ASSERT_EQ(IsFileHandlingEnabled(),
              fake_os_integration().IsFileExtensionHandled(
                  profile(), app_id, app_name, extension));
  }

  SynchronizeOsOptions options;
  options.force_unregister_os_integration = true;
  test::SynchronizeOsIntegration(profile(), app_id, options);

  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    EXPECT_FALSE(fake_os_integration().IsFileExtensionHandled(
        profile(), app_id, app_name, extension));
  }
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest,
       ForceUnregisterAppNotInRegistry) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);
  const std::string& app_name =
      provider().registrar_unsafe().GetAppShortName(app_id);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  proto::os_state::WebAppOsIntegration os_integration_state = state.value();
  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    EXPECT_EQ(IsFileHandlingEnabled(),
              fake_os_integration().IsFileExtensionHandled(
                  profile(), app_id, app_name, extension));
  }

  test::UninstallAllWebApps(profile());
  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    EXPECT_FALSE(fake_os_integration().IsFileExtensionHandled(
        profile(), app_id, app_name, extension));
  }
  EXPECT_FALSE(provider().registrar_unsafe().IsInRegistrar(app_id));

  // The file handling continues to not be registered.
  SynchronizeOsOptions options;
  options.force_unregister_os_integration = true;
  test::SynchronizeOsIntegration(profile(), app_id, options);
  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    EXPECT_FALSE(fake_os_integration().IsFileExtensionHandled(
        profile(), app_id, app_name, extension));
  }
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest, NoHandlersRegistered) {
  // Returns an empty list when no file handlers are registered.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls("app", {path});
  EXPECT_TRUE(launch_infos.empty());
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest, NoLaunchFilesPassed) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  // Returns an empty list when no launch files are passed.
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id, {});
  EXPECT_TRUE(launch_infos.empty());
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest,
       SingleValidExtensionSingleExtensionHandler) {
  apps::FileHandlers file_handlers;
  GURL file_handler_url("https://app.site/open-foo");

  {
    apps::FileHandler file_handler;
    file_handler.action = file_handler_url;
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  // Matches on single valid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id, {path});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(file_handler_url, std::get<GURL>(launch_infos[0]));
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest,
       ExtensionCaseInsensitive) {
  apps::FileHandlers file_handlers;
  GURL file_handler_url("https://app.site/open-foo");

  {
    apps::FileHandler file_handler;
    file_handler.action = file_handler_url;
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  // Matches on single valid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.FOO"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id, {path});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(file_handler_url, std::get<GURL>(launch_infos[0]));
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest,
       SingleInvalidExtensionSingleExtensionHandler) {
  apps::FileHandlers file_handlers;
  GURL file_handler_url("https://app.site/open-foo");

  {
    apps::FileHandler file_handler;
    file_handler.action = file_handler_url;
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  // Returns nullopt on single invalid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.bar"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id, {path});
  EXPECT_TRUE(launch_infos.empty());
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest,
       SingleValidExtensionMultiExtensionHandler) {
  apps::FileHandlers file_handlers;
  GURL file_handler_url("https://app.site/open-foo");

  {
    apps::FileHandler file_handler;
    file_handler.action = file_handler_url;
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
      accept_entry.mime_type = "application/bar";
      accept_entry.file_extensions.insert(".bar");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  // Matches on single valid extension for multi-extension handler.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id, {path});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(file_handler_url, std::get<GURL>(launch_infos[0]));
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest, MultipleValidExtensions) {
  apps::FileHandlers file_handlers;
  GURL file_handler_url("https://app.site/open-foo");

  {
    apps::FileHandler file_handler;
    file_handler.action = file_handler_url;
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
      accept_entry.mime_type = "application/bar";
      accept_entry.file_extensions.insert(".bar");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  // Matches on multiple valid extensions for multi-extension handler.
  const base::FilePath path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.bar"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id, {path1, path2});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(file_handler_url, std::get<GURL>(launch_infos[0]));
  const auto& paths = std::get<std::vector<base::FilePath>>(launch_infos[0]);
  EXPECT_EQ(2u, paths.size());
  EXPECT_TRUE(base::Contains(paths, path1));
  EXPECT_TRUE(base::Contains(paths, path2));
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest, PartialExtensionMatch) {
  apps::FileHandlers file_handlers;
  GURL file_handler_url("https://app.site/open-foo");

  {
    apps::FileHandler file_handler;
    file_handler.action = file_handler_url;
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  // Works with partial extension match.
  const base::FilePath path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.bar"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id, {path1, path2});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(file_handler_url, std::get<GURL>(launch_infos[0]));
  const auto& paths = std::get<std::vector<base::FilePath>>(launch_infos[0]);
  EXPECT_EQ(1u, paths.size());
  EXPECT_TRUE(base::Contains(paths, path1));
  EXPECT_FALSE(base::Contains(paths, path2));
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest,
       SingleFileWithoutExtension) {
  apps::FileHandlers file_handlers;
  GURL file_handler_url("https://app.site/open-foo");

  {
    apps::FileHandler file_handler;
    file_handler.action = file_handler_url;
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  // Returns nullopt where a file has no extension.
  const base::FilePath path(FILE_PATH_LITERAL("file"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id, {path});
  EXPECT_TRUE(launch_infos.empty());
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest,
       FileWithoutExtensionAmongMultipleFiles) {
  apps::FileHandlers file_handlers;
  GURL file_handler_url("https://app.site/open-foo");

  {
    apps::FileHandler file_handler;
    file_handler.action = file_handler_url;
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  // Returns nullopt where one file has no extension while others do.
  const base::FilePath path1(FILE_PATH_LITERAL("file"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.foo"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id, {path1, path2});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(file_handler_url, std::get<GURL>(launch_infos[0]));
  const auto& paths = std::get<std::vector<base::FilePath>>(launch_infos[0]);
  EXPECT_EQ(1u, paths.size());
  EXPECT_FALSE(base::Contains(paths, path1));
  EXPECT_TRUE(base::Contains(paths, path2));
}

TEST_F(FileHandlingSubManagerConfigureAndExecuteTest, MultiLaunch) {
  const GURL foo_url("https://app.site/handle-foo");
  const GURL bar_url("https://app.site/handle-bar");
  // Baz will have multiple clients.
  const GURL baz_url("https://app.site/handle-baz");

  apps::FileHandlers file_handlers;
  {
    apps::FileHandler file_handler;
    file_handler.action = foo_url;
    file_handler.display_name = u"Foo opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
    file_handler.action = bar_url;
    file_handler.display_name = u"Bar opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/bar";
      accept_entry.file_extensions.insert(".bar");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
    file_handler.action = baz_url;
    file_handler.display_name = u"Baz opener";
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/baz";
      accept_entry.file_extensions.insert(".baz");
      file_handler.accept.push_back(accept_entry);
    }
    file_handler.launch_type = apps::FileHandler::LaunchType::kMultipleClients;
    file_handlers.push_back(file_handler);
  }

  const webapps::AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  // Finds multiple different handlers for multiple different file types, but
  // coalesces matching file types.
  const base::FilePath foo_path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath foo_path2(FILE_PATH_LITERAL("file2.foo"));
  const base::FilePath bar_path(FILE_PATH_LITERAL("file.bar"));
  const base::FilePath baz_path1(FILE_PATH_LITERAL("file.baz"));
  const base::FilePath baz_path2(FILE_PATH_LITERAL("file2.baz"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(
          app_id, {foo_path1, foo_path2, bar_path, baz_path1, baz_path2});
  ASSERT_EQ(4u, launch_infos.size());
  // The expected number of launches for each action URL.
  std::map<GURL, int> expected_counts;
  expected_counts[foo_url] = 1;
  expected_counts[bar_url] = 1;
  expected_counts[baz_url] = 2;
  for (int i = 0; i < 4; ++i) {
    GURL launch_url = std::get<GURL>(launch_infos[i]);
    const auto& launch_paths =
        std::get<std::vector<base::FilePath>>(launch_infos[i]);
    expected_counts[launch_url]--;
    if (launch_url == foo_url) {
      EXPECT_EQ(2u, launch_paths.size());
      EXPECT_TRUE(base::Contains(launch_paths, foo_path1));
      EXPECT_TRUE(base::Contains(launch_paths, foo_path2));
    } else if (launch_url == bar_url) {
      EXPECT_EQ(1u, launch_paths.size());
      EXPECT_TRUE(base::Contains(launch_paths, bar_path));
    } else if (launch_url == baz_url) {
      EXPECT_EQ(1u, launch_paths.size());
      bool has_path1 = base::Contains(launch_paths, baz_path1);
      bool has_path2 = base::Contains(launch_paths, baz_path2);
      EXPECT_NE(has_path1, has_path2);
    } else {
      FAIL() << " Got unexpected URL " << launch_url;
    }
  }

  for (const auto& launch_count : expected_counts) {
    EXPECT_EQ(0, launch_count.second)
        << " Didn't see enough launches for " << launch_count.first.spec();
  }
}

}  // namespace

}  // namespace web_app
