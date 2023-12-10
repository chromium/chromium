// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/install_placeholder_job.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/mock_data_retriever.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

class InstallPlaceholderJobWrapperCommand
    : public WebAppCommandTemplate<SharedWebContentsWithAppLock> {
 public:
  InstallPlaceholderJobWrapperCommand(
      Profile* profile,
      const ExternalInstallOptions& install_options,
      InstallPlaceholderJob::InstallAndReplaceCallback callback,
      std::unique_ptr<WebAppDataRetriever> data_retriever = nullptr)
      : WebAppCommandTemplate<SharedWebContentsWithAppLock>(
            "InstallPlaceholderJobWrapperCommand"),
        profile_(*profile),
        install_options_(install_options),
        callback_(std::move(callback)),
        data_retriever_(std::move(data_retriever)),
        lock_description_(
            std::make_unique<SharedWebContentsWithAppLockDescription>(
                base::flat_set<webapps::AppId>{
                    GenerateAppId(/*manifest_id_path=*/absl::nullopt,
                                  install_options.install_url)})) {}

  ~InstallPlaceholderJobWrapperCommand() override = default;

  void StartWithLock(
      std::unique_ptr<SharedWebContentsWithAppLock> lock) override {
    lock_ = std::move(lock);
    install_placeholder_job_ = std::make_unique<InstallPlaceholderJob>(
        &*profile_, install_options_,
        base::BindOnce(
            &InstallPlaceholderJobWrapperCommand::OnPlaceholderInstalled,
            weak_factory_.GetWeakPtr()),
        *lock_);
    if (data_retriever_) {
      install_placeholder_job_->SetDataRetrieverForTesting(
          std::move(data_retriever_));
    }
    install_placeholder_job_->Start();
  }

  void OnPlaceholderInstalled(webapps::InstallResultCode code,
                              webapps::AppId app_id) {
    SignalCompletionAndSelfDestruct(
        webapps::IsSuccess(code) ? CommandResult::kSuccess
                                 : CommandResult::kFailure,
        base::BindOnce(std::move(callback_), code, std::move(app_id)));
  }

  const LockDescription& lock_description() const override {
    return *lock_description_;
  }

  base::Value ToDebugValue() const override { return base::Value(); }

  void OnShutdown() override {}

 private:
  raw_ref<Profile> profile_;
  ExternalInstallOptions install_options_;
  InstallPlaceholderJob::InstallAndReplaceCallback callback_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  std::unique_ptr<SharedWebContentsWithAppLockDescription> lock_description_;
  std::unique_ptr<SharedWebContentsWithAppLock> lock_;

  std::unique_ptr<InstallPlaceholderJob> install_placeholder_job_;

  base::WeakPtrFactory<InstallPlaceholderJobWrapperCommand> weak_factory_{this};
};

class InstallPlaceholderJobTest : public WebAppTest {
 public:
  static constexpr int kIconSize = 96;
  const GURL kInstallUrl = GURL("https://example.com");

  void SetUp() override {
    WebAppTest::SetUp();
    auto shortcut_manager = std::make_unique<TestShortcutManager>(profile());
    shortcut_manager_ = shortcut_manager.get();
    FakeWebAppProvider::Get(profile())
        ->GetOsIntegrationManager()
        .AsTestOsIntegrationManager()
        ->SetShortcutManager(std::move(shortcut_manager));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    shortcut_manager_ = nullptr;
    WebAppTest::TearDown();
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  FakeOsIntegrationManager& fake_os_integration_manager() {
    return static_cast<FakeOsIntegrationManager&>(
        provider()->os_integration_manager());
  }

  TestShortcutManager* shortcut_manager() { return shortcut_manager_; }

 private:
  raw_ptr<TestShortcutManager> shortcut_manager_;
};

TEST_F(InstallPlaceholderJobTest, InstallPlaceholder) {
  ExternalInstallOptions options(kInstallUrl, mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  base::test::TestFuture<webapps::InstallResultCode, webapps::AppId> future;

  provider()->command_manager().ScheduleCommand(
      std::make_unique<InstallPlaceholderJobWrapperCommand>(
          profile(), options, future.GetCallback()));

  EXPECT_EQ(future.Get<0>(), webapps::InstallResultCode::kSuccessNewInstall);
  const webapps::AppId app_id = future.Get<1>();
  EXPECT_TRUE(provider()->registrar_unsafe().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));
  EXPECT_EQ(fake_os_integration_manager().num_create_shortcuts_calls(), 1u);
  auto last_install_options =
      fake_os_integration_manager().get_last_install_options();
  EXPECT_TRUE(last_install_options->add_to_desktop);
  EXPECT_TRUE(last_install_options->add_to_quick_launch_bar);
  EXPECT_FALSE(last_install_options->os_hooks[OsHookType::kRunOnOsLogin]);
  if (AreOsIntegrationSubManagersEnabled()) {
    absl::optional<proto::WebAppOsIntegrationState> os_state =
        provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
    ASSERT_TRUE(os_state.has_value());
    EXPECT_TRUE(os_state->has_shortcut());
    EXPECT_EQ(os_state->run_on_os_login().run_on_os_login_mode(),
              proto::RunOnOsLoginMode::NOT_RUN);
  }
}

TEST_F(InstallPlaceholderJobTest, InstallPlaceholderWithOverrideIconUrl) {
  ExternalInstallOptions options(kInstallUrl, mojom::UserDisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  const GURL icon_url("https://example.com/test.png");
  options.override_icon_url = icon_url;
  base::test::TestFuture<webapps::InstallResultCode, webapps::AppId> future;

  auto data_retriever =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();

  bool skip_page_favicons = true;
  bool fail_all_if_any_fail = false;
  SkBitmap bitmap;
  std::vector<gfx::Size> icon_sizes(1, gfx::Size(kIconSize, kIconSize));
  bitmap.allocN32Pixels(kIconSize, kIconSize);
  bitmap.eraseColor(SK_ColorRED);
  IconsMap icons = {{icon_url, {bitmap}}};
  DownloadedIconsHttpResults http_result = {
      {icon_url, net::HttpStatusCode::HTTP_OK}};
  EXPECT_CALL(
      *data_retriever,
      GetIcons(testing::_, testing::ElementsAre(icon_url), skip_page_favicons,
               fail_all_if_any_fail, base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<4>(
          IconsDownloadedResult::kCompleted, std::move(icons), http_result));

  auto command = std::make_unique<InstallPlaceholderJobWrapperCommand>(
      profile(), options, future.GetCallback(), std::move(data_retriever));
  provider()->command_manager().ScheduleCommand(std::move(command));

  EXPECT_EQ(future.Get<0>(), webapps::InstallResultCode::kSuccessNewInstall);
  const webapps::AppId app_id = future.Get<1>();
  EXPECT_TRUE(provider()->registrar_unsafe().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));
  EXPECT_EQ(fake_os_integration_manager().num_create_shortcuts_calls(), 1u);
  if (AreOsIntegrationSubManagersEnabled()) {
    absl::optional<proto::WebAppOsIntegrationState> os_state =
        provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
    ASSERT_TRUE(os_state.has_value());
    EXPECT_TRUE(os_state->has_shortcut());
  }
}

}  // namespace
}  // namespace web_app
