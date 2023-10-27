// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_ash_chrome_browser_main_extra_parts.h"

#include "ash/test/ui_controls_ash.h"
#include "ash/multi_device_setup/multi_device_notification_presenter.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/test_controller_ash.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "ui/views/input_event_activation_protector.h"

namespace test {

// The file path to indicate if ash is ready for testing.
// The file should not be on the file system initially. After
// ash is ready for testing, the file will be created.
constexpr char kAshReadyFilePathFlag[] = "ash-ready-file-path";

TestAshChromeBrowserMainExtraParts::TestAshChromeBrowserMainExtraParts()
    : test_controller_ash_(std::make_unique<crosapi::TestControllerAsh>()) {}

TestAshChromeBrowserMainExtraParts::
    ~TestAshChromeBrowserMainExtraParts() = default;

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

void TestAshChromeBrowserMainExtraParts::PreProfileInit() {
  crosapi::BrowserManager::DisableForTesting();
  // TODO(crbug.com/1422469): Explore whether there is a better place to disable
  // the built-in tts engine other than TestAshChromeBrowserMainExtraParts,
  // which may make test_ash_chrome behavior differs from production ash chrome.
  TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
}

void TestAshChromeBrowserMainExtraParts::PreBrowserStart() {
  // These are used by exo's weston-test protocol for event injection.
  // TODO(oshima): Move this to the test protocol side.
  ash::test::EnableUIControlsAsh();
}

void TestAshChromeBrowserMainExtraParts::PostBrowserStart() {
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

  ignore_signin_errors_ =
      ash::SigninErrorNotifier::IgnoreSyncErrorsForTesting();
  ignore_multi_device_notifications_ =
      ash::MultiDeviceNotificationPresenter::DisableNotificationsForTesting();

  // Call this at the end of PostBrowserStart().
  AshIsReadyForTesting();
}

void TestAshChromeBrowserMainExtraParts::PostMainMessageLoopRun() {
  crosapi::CrosapiManager::Get()->crosapi_ash()->SetTestControllerForTesting(
      nullptr);
}

}  // namespace test
