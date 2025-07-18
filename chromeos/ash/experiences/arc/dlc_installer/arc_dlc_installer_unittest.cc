// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_installer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/fake_cros_settings_provider.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_notification_manager.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_dlc_install_hardware_checker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"

namespace arc {

class ArcDlcInstallerTest : public testing::Test {
 protected:
  void SetUp() override {
    ash::DlcserviceClient::InitializeFake();
    ash::UpstartClient::InitializeFake();

    auto fake_message_center =
        std::make_unique<message_center::FakeMessageCenter>();
    fake_message_center_ = fake_message_center.get();
    message_center::MessageCenter::InitializeForTesting(
        std::move(fake_message_center));

    std::unique_ptr<FakeArcDlcInstallHardwareChecker> fake_hardware_checker =
        std::make_unique<FakeArcDlcInstallHardwareChecker>(true);
    cros_settings_ = std::make_unique<ash::CrosSettings>();
    auto provider =
        std::make_unique<ash::FakeCrosSettingsProvider>(base::DoNothing());
    fake_provider_ = provider.get();
    cros_settings_->AddSettingsProvider(std::move(provider));
    // TODO(b/405341089): Update fake provider to accept unset value for
    // specific path.
    fake_provider_->Set(ash::kDeviceFlexArcPreloadEnabled, base::Value());
    arc_dlc_installer_ = std::make_unique<ArcDlcInstaller>(
        std::move(fake_hardware_checker), cros_settings_.get());
  }

  void TearDown() override {
    arc_dlc_installer_.reset();
    fake_provider_ = nullptr;
    cros_settings_.reset();
    ash::UpstartClient::Shutdown();
    ash::DlcserviceClient::Shutdown();
    fake_message_center_ = nullptr;
    message_center::MessageCenter::Shutdown();
  }

  void SetFlexArcPreloadEnabled(bool enabled) {
    fake_provider_->Set(ash::kDeviceFlexArcPreloadEnabled,
                        base::Value(enabled));
  }

  void InstallArcImageFromDlc(std::string_view install_error) {
    // Configure the FakeDlcserviceClient to trigger the progress callback.
    fake_dlcservice_client()->set_trigger_install_progress(true);
    fake_dlcservice_client()->set_install_error(install_error);

    base::RunLoop run_loop;
    auto callback = base::IgnoreArgs<bool>(run_loop.QuitClosure());

    arc_dlc_installer_->PrepareArc(std::move(callback));
    run_loop.Run();
  }

  void VerifyNotifications(base::span<const std::string_view> expected_ids) {
    const auto& notifications = fake_message_center_->GetNotifications();
    ASSERT_EQ(notifications.size(), expected_ids.size());
    size_t index = 0;
    for (const auto& notification : notifications) {
      ASSERT_TRUE(notification);
      EXPECT_EQ(notification->id(), expected_ids[index++]);
    }
  }

  ash::FakeDlcserviceClient* fake_dlcservice_client() {
    return static_cast<ash::FakeDlcserviceClient*>(
        ash::DlcserviceClient::Get());
  }

  base::test::TaskEnvironment task_environment_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
  raw_ptr<message_center::FakeMessageCenter> fake_message_center_ = nullptr;
  std::unique_ptr<ash::CrosSettings> cros_settings_;
  raw_ptr<ash::FakeCrosSettingsProvider> fake_provider_ = nullptr;
  std::unique_ptr<ArcDlcInstaller> arc_dlc_installer_;
};

// Verify that the hardware check is not being run to install
// the arcvm DLC image when Reven branding is disabled
TEST_F(ArcDlcInstallerTest, MaybeEnableArc_NonRevenBranding) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);
  arc_dlc_installer_->PrepareArc(
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
}

// Verify that the hardware check is not being run to install
// the arcvm DLC image for unmanaged devices.
TEST_F(ArcDlcInstallerTest, MaybeEnableArc_UnmanagedDevice) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kRevenBranding);
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);
  arc_dlc_installer_->PrepareArc(
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
}

// Verify that the hardware check is not run to install
// the ARCVM DLC image when the kDeviceFlexArcPreloadEnabled policy is unset.
TEST_F(ArcDlcInstallerTest, MaybeEnableArc_WithPolicyUnset) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kRevenBranding);
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  arc_dlc_installer_->PrepareArc(
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
}

// Verify that the hardware check is not being run to install
// the arcvm DLC image when kDeviceFlexArcPreloadEnabled policy is off.
TEST_F(ArcDlcInstallerTest, MaybeEnableArc_WithPolicyOff) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kRevenBranding);
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(false);
  arc_dlc_installer_->PrepareArc(
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
}

TEST_F(ArcDlcInstallerTest, VerifyNotifications_InstallSuccess) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kRevenBranding);
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);

  InstallArcImageFromDlc(dlcservice::kErrorNone);
  VerifyNotifications(
      {arc_dlc_install_notification_manager::kArcVmPreloadSucceededId,
       arc_dlc_install_notification_manager::kArcVmPreloadStartedId});
}

TEST_F(ArcDlcInstallerTest, VerifyNotifications_InstallFail) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kRevenBranding);
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);

  InstallArcImageFromDlc(dlcservice::kErrorInternal);
  VerifyNotifications(
      {arc_dlc_install_notification_manager::kArcVmPreloadFailedId,
       arc_dlc_install_notification_manager::kArcVmPreloadStartedId});
}

// Verifies that installation completion notifications are triggered only once
// even after repeated DLC installations.
TEST_F(ArcDlcInstallerTest, CompletionNotificationTriggerOnce_RepeatInstall) {
  test_install_attributes_.Get()->SetCloudManaged("example.com",
                                                  "fake-device-id");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kRevenBranding);
  // Add arcvm-dlc command flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVmDlc);
  SetFlexArcPreloadEnabled(true);

  // Simulate the first DLC installation.
  InstallArcImageFromDlc(dlcservice::kErrorNone);

  // Simulate the second DLC installation.
  InstallArcImageFromDlc(dlcservice::kErrorNone);

  // Expect two notifications: one for the preload start and one for the
  // success, even after triggering the installation twice.
  VerifyNotifications(
      {arc_dlc_install_notification_manager::kArcVmPreloadSucceededId,
       arc_dlc_install_notification_manager::kArcVmPreloadStartedId});
}

}  // namespace arc
