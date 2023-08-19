// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/file_handling_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

class FileHandlingSubManagerConfigureTest
    : public WebAppTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

  FileHandlingSubManagerConfigureTest() = default;
  ~FileHandlingSubManagerConfigureTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ =
          OsIntegrationTestOverrideImpl::OverrideForTesting(base::GetHomeDir());
    }
    InitScopedFeatureList(scoped_feature_list_);

    provider_ = FakeWebAppProvider::Get(profile());

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto shortcut_manager = std::make_unique<WebAppShortcutManager>(
        profile(), file_handler_manager.get(), protocol_handler_manager.get());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(shortcut_manager), std::move(file_handler_manager),
        std::move(protocol_handler_manager), /*url_handler_manager=*/nullptr);

    provider_->SetOsIntegrationManager(std::move(os_integration_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    // Blocking required due to file operations in the shortcut override
    // destructor.
    test::UninstallAllWebApps(profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppTest::TearDown();
  }

  web_app::AppId InstallWebAppWithFileHandlers(
      apps::FileHandlers file_handlers) {
    std::unique_ptr<WebAppInstallInfo> info =
        std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    info->file_handlers = file_handlers;
    base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
    // InstallFromInfoWithParams is used instead of InstallFromInfo, because
    // InstallFromInfo doesn't register OS integration.
    provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<AppId>();
  }

 protected:
  WebAppProvider& provider() { return *provider_; }
  scoped_refptr<OsIntegrationTestOverrideImpl> test_override() {
    return test_override_->test_override;
  }

 private:
  virtual void InitScopedFeatureList(
      base::test::ScopedFeatureList& scoped_feature_list) {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers, {{"stage", "write_config"}});
    } else {
      scoped_feature_list.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }
  }

  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

TEST_P(FileHandlingSubManagerConfigureTest, InstallWithFilehandlers) {
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

  const AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled() &&
      ShouldRegisterFileHandlersWithOs()) {
    ASSERT_TRUE(os_integration_state.has_file_handling());
    auto file_handling = os_integration_state.file_handling();

    ASSERT_EQ(file_handling.file_handlers_size(), 2);

    EXPECT_EQ(file_handling.file_handlers(0).accept_size(), 2);
    EXPECT_EQ(file_handling.file_handlers(0).display_name(), "Foo opener");
    EXPECT_EQ(file_handling.file_handlers(0).action(),
              "https://app.site/open-foo");
    EXPECT_EQ(file_handling.file_handlers(0).accept(0).mimetype(),
              "application/foo");
    EXPECT_EQ(file_handling.file_handlers(0).accept(0).file_extensions_size(),
              1);
    EXPECT_EQ(file_handling.file_handlers(0).accept(0).file_extensions(0),
              ".foo");
    EXPECT_EQ(file_handling.file_handlers(0).accept(1).mimetype(),
              "application/foobar");
    EXPECT_EQ(file_handling.file_handlers(0).accept(1).file_extensions_size(),
              1);
    EXPECT_EQ(file_handling.file_handlers(0).accept(1).file_extensions(0),
              ".foobar");

    EXPECT_EQ(file_handling.file_handlers(1).accept_size(), 1);
    EXPECT_EQ(file_handling.file_handlers(1).display_name(), "Bar opener");
    EXPECT_EQ(file_handling.file_handlers(1).action(),
              "https://app.site/open-bar");
    EXPECT_EQ(file_handling.file_handlers(1).accept(0).mimetype(),
              "application/bar");
    EXPECT_EQ(file_handling.file_handlers(1).accept(0).file_extensions_size(),
              2);
    EXPECT_EQ(file_handling.file_handlers(1).accept(0).file_extensions(0),
              ".bar");
    EXPECT_EQ(file_handling.file_handlers(1).accept(0).file_extensions(1),
              ".baz");
  } else {
    ASSERT_FALSE(os_integration_state.has_file_handling());
  }
}

TEST_P(FileHandlingSubManagerConfigureTest, UpdateUserChoiceDisallowed) {
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

  const AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  base::test::TestFuture<void> future;
  provider().scheduler().PersistFileHandlersUserChoice(
      app_id, /*allowed=*/false, future.GetCallback());

  ASSERT_TRUE(future.Wait());

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  ASSERT_FALSE(os_integration_state.has_file_handling());
}

TEST_P(FileHandlingSubManagerConfigureTest, Uninstall) {
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

  const AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  test::UninstallAllWebApps(profile());
  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_FALSE(state.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileHandlingSubManagerConfigureTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

class FileHandlingSubManagerConfigureAndExecuteTest
    : public FileHandlingSubManagerConfigureTest {
 private:
  void InitScopedFeatureList(
      base::test::ScopedFeatureList& scoped_feature_list) override {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers, {{"stage", "write_config"}});
    } else if (GetParam() ==
               OsIntegrationSubManagersState::kSaveStateAndExecute) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers,
          {{"stage", "execute_and_write_config"}});
    } else {
      scoped_feature_list.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }
  }
};

TEST_P(FileHandlingSubManagerConfigureAndExecuteTest, InstallWithFilehandlers) {
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

  const AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    ASSERT_TRUE(test_override()->IsFileExtensionHandled(
        profile(), app_id,
        provider().registrar_unsafe().GetAppShortName(app_id), extension));
  }
}

TEST_P(FileHandlingSubManagerConfigureAndExecuteTest,
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

  const AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);
  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();

  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    ASSERT_TRUE(test_override()->IsFileExtensionHandled(
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
  const proto::WebAppOsIntegrationState& new_os_integration_state =
      new_state.value();
  ASSERT_FALSE(new_os_integration_state.has_file_handling());

  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    ASSERT_FALSE(test_override()->IsFileExtensionHandled(
        profile(), app_id,
        provider().registrar_unsafe().GetAppShortName(app_id), extension));
  }
}

TEST_P(FileHandlingSubManagerConfigureAndExecuteTest, Uninstall) {
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

  const AppId& app_id = InstallWebAppWithFileHandlers(file_handlers);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    ASSERT_TRUE(test_override()->IsFileExtensionHandled(
        profile(), app_id,
        provider().registrar_unsafe().GetAppShortName(app_id), extension));
  }
  test::UninstallAllWebApps(profile());
  auto new_state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_FALSE(new_state.has_value());
  for (const auto& extension : GetFileExtensionsFromFileHandlingProto(
           os_integration_state.file_handling())) {
    ASSERT_FALSE(test_override()->IsFileExtensionHandled(
        profile(), app_id,
        provider().registrar_unsafe().GetAppShortName(app_id), extension));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileHandlingSubManagerConfigureAndExecuteTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kSaveStateAndExecute,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace

}  // namespace web_app
