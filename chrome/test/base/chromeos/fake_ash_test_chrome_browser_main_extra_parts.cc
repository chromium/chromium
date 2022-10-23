// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ash_test_chrome_browser_main_extra_parts.h"

#include "ash/multi_device_setup/multi_device_notification_presenter.h"
#include "ash/test/ui_controls_factory_ash.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/test_controller_ash.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/input_event_activation_protector.h"

namespace test {

// The file path to indicate if ash is ready for testing.
// The file should not be on the file system initially. After
// ash is ready for testing, the file will be created.
constexpr char kAshReadyFilePathFlag[] = "ash-ready-file-path";

FakeAshTestChromeBrowserMainExtraParts::FakeAshTestChromeBrowserMainExtraParts()
    : test_controller_ash_(std::make_unique<crosapi::TestControllerAsh>()) {
  base::test::AllowCheckIsTestForTesting();
}

FakeAshTestChromeBrowserMainExtraParts::
    ~FakeAshTestChromeBrowserMainExtraParts() = default;

// Create a file so test_runner know ash is ready for testing.
void AshIsReadyForTesting() {
  // TODO(crbug.com/1107966) Remove this early return after
  // test_runner make related changes.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kAshReadyFilePathFlag)) {
    return;
  }

  CHECK(
      base::CommandLine::ForCurrentProcess()->HasSwitch(kAshReadyFilePathFlag));
  base::FilePath path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kAshReadyFilePathFlag);
  CHECK(!base::PathExists(path));
  CHECK(base::WriteFile(path, "ash is ready"));
}

void FakeAshTestChromeBrowserMainExtraParts::PreProfileInit() {
  crosapi::BrowserManager::DisableForTesting();
}

void FakeAshTestChromeBrowserMainExtraParts::PreBrowserStart() {
  // These are used by exo's weston-test protocol for event injection.
  ui_controls::InstallUIControlsAura(ash::test::CreateAshUIControls());
}

void FakeAshTestChromeBrowserMainExtraParts::PostBrowserStart() {
  // Fake ML service is needed because ml service client library
  // requires the ml service daemon, which is not present in the
  // unit test or browser test environment.
  auto* fake_service_connection =
      new chromeos::machine_learning::FakeServiceConnectionImpl();
  fake_service_connection->Initialize();
  chromeos::machine_learning::ServiceConnection::
      UseFakeServiceConnectionForTesting(fake_service_connection);

  crosapi::CrosapiManager::Get()->crosapi_ash()->SetTestControllerForTesting(
      test_controller_ash_.get());
  views::InputEventActivationProtector::DisableForTesting();

  ignore_signin_errors_ =
      ash::SigninErrorNotifier::IgnoreSyncErrorsForTesting();
  ignore_multi_device_notifications_ =
      ash::MultiDeviceNotificationPresenter::DisableNotificationsForTesting();

  // Call this at the end of PostBrowserStart().
  AshIsReadyForTesting();
}

void FakeAshTestChromeBrowserMainExtraParts::PostMainMessageLoopRun() {
  crosapi::CrosapiManager::Get()->crosapi_ash()->SetTestControllerForTesting(
      nullptr);
}

}  // namespace test
