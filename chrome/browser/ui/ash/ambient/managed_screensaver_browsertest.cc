// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/autotest_ambient_api.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

const char kRedImageFileName[] = "chromeos/screensaver/red.jpg";
const char kGreenImageFileName[] = "chromeos/screensaver/green.jpg";
const char kBlueImageFileName[] = "chromeos/screensaver/blue.jpg";

class ManagedScreensaverBrowserTest : public LoginManagerTest {
 public:
  ManagedScreensaverBrowserTest()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitAndEnableFeature(
        ash::features::kAmbientModeManagedScreensaver);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    login_manager_mixin_.AppendManagedUsers(1);
  }
  ~ManagedScreensaverBrowserTest() override = default;

  // LoginManagerTest overrides:
  void SetUpInProcessBrowserTestFixture() override {
    device_policy_.Build();
    OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
        owner_key_util_);
    // Set the signing key in the owner key util
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    // Override FakeSessionManagerClient.
    SessionManagerClient::InitializeFakeInMemory();

    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_.GetBlob());

    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }

  void InitializeForLoginScreen() {
    SetDevicePolicyEnabled(true);

    // Set intervals to zero so that we don't rely on time during testing.
    SetDevicePolicyImageDisplayIntervalSeconds(0);
    SetDevicePolicyScreenIdleTimeoutSeconds(0);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    // Allow failing policy fetch so that we don't shutdown the profile on
    // failure.
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUp() override {
    // Setup the HTTPS test server
    https_server_.ServeFilesFromDirectory(GetChromeTestDataDir());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);

    ASSERT_TRUE(https_server_.InitializeAndListen());

    LoginManagerTest::SetUp();
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    https_server_.StartAcceptingConnections();
  }

  void SetDevicePolicyImages(const std::vector<std::string>& images) {
    if (images.empty()) {
      device_policy_.payload().Clear();
      BuildDevicePolicyAndNotify();
      return;
    }

    enterprise_management::DeviceScreensaverLoginScreenImagesProto*
        mutable_images = device_policy_.payload()
                             .mutable_device_screensaver_login_screen_images();
    for (const auto& image_path : images) {
      mutable_images->add_device_screensaver_login_screen_images(
          https_server_.GetURL("/" + image_path).spec());
    }
    BuildDevicePolicyAndNotify();
  }

  void SetDevicePolicyEnabled(bool enabled) {
    if (enabled) {
      device_policy_.payload()
          .mutable_device_screensaver_login_screen_enabled()
          ->set_device_screensaver_login_screen_enabled(enabled);
    } else {
      device_policy_.payload().Clear();
    }
    BuildDevicePolicyAndNotify();
  }

  void SetDevicePolicyImageDisplayIntervalSeconds(int64_t interval) {
    device_policy_.payload()
        .mutable_device_screensaver_login_screen_image_display_interval_seconds()
        ->set_device_screensaver_login_screen_image_display_interval_seconds(
            interval);

    BuildDevicePolicyAndNotify();
  }

  void SetDevicePolicyScreenIdleTimeoutSeconds(int64_t timeout) {
    device_policy_.payload()
        .mutable_device_screensaver_login_screen_idle_timeout_seconds()
        ->set_device_screensaver_login_screen_idle_timeout_seconds(timeout);
    BuildDevicePolicyAndNotify();
  }

  void BuildDevicePolicyAndNotify() {
    device_policy_.Build();
    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_.GetBlob());
    FakeSessionManagerClient::Get()->OnPropertyChangeComplete(
        /*success=*/true);
  }

  views::View* GetContainerView() {
    auto* widget =
        Shell::GetPrimaryRootWindowController()->ambient_widget_for_testing();

    if (widget) {
      auto* container_view = widget->GetContentsView();
      DCHECK(container_view &&
             container_view->GetID() == kAmbientContainerView);
      return container_view;
    }
    return nullptr;
  }

 protected:
  std::unique_ptr<base::RunLoop> run_loop_;
  base::test::ScopedFeatureList feature_list_;
  policy::DevicePolicyBuilder device_policy_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  net::test_server::EmbeddedTestServer https_server_;

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ManagedScreensaverBrowserTest, BasicTest) {
  InitializeForLoginScreen();
  SetDevicePolicyImages(
      {kRedImageFileName, kBlueImageFileName, kGreenImageFileName});
  run_loop_ = std::make_unique<base::RunLoop>();
  AutotestAmbientApi test_api;
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/3, /*timeout=*/base::Seconds(6),
      /*on_complete=*/run_loop_->QuitClosure(),
      /*on_timeout=*/base::BindOnce([]() { NOTREACHED(); }));
  run_loop_->Run();
  ASSERT_NE(nullptr, GetContainerView());
}

IN_PROC_BROWSER_TEST_F(ManagedScreensaverBrowserTest,
                       OneImageDoesNotStartAmbientMode) {
  InitializeForLoginScreen();
  SetDevicePolicyImages({kRedImageFileName});

  AutotestAmbientApi test_api;
  run_loop_ = std::make_unique<base::RunLoop>();
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/1, /*timeout=*/base::Seconds(2),
      /*on_complete=*/base::BindOnce([]() { NOTREACHED(); }),
      /*on_timeout=*/run_loop_->QuitClosure());
  run_loop_->Run();

  ASSERT_EQ(nullptr, GetContainerView());
}

}  // namespace ash
