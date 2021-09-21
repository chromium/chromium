// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ash_test_chrome_browser_main_extra_parts.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"

namespace test {

// The file path to indicate if ash is ready for testing.
// The file should not be on the file system initially. After
// ash is ready for testing, the file will be created.
constexpr char kAshReadyFilePathFlag[] = "ash-ready-file-path";

FakeAshTestChromeBrowserMainExtraParts::
    FakeAshTestChromeBrowserMainExtraParts() = default;

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

void FakeAshTestChromeBrowserMainExtraParts::PostBrowserStart() {
  // Fake ML service is needed because ml service client library
  // requires the ml service daemon, which is not present in the
  // unit test or browser test environment.
  auto* fake_service_connection =
      new chromeos::machine_learning::FakeServiceConnectionImpl();
  fake_service_connection->Initialize();
  chromeos::machine_learning::ServiceConnection::
      UseFakeServiceConnectionForTesting(fake_service_connection);

  // Call this at the end of PostBrowserStart().
  AshIsReadyForTesting();
}

}  // namespace test
