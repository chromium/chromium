// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ash_test_chrome_browser_main_extra_parts.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"

namespace test {

FakeAshTestChromeBrowserMainExtraParts::
    FakeAshTestChromeBrowserMainExtraParts() = default;

FakeAshTestChromeBrowserMainExtraParts::
    ~FakeAshTestChromeBrowserMainExtraParts() = default;

void FakeAshTestChromeBrowserMainExtraParts::PostBrowserStart() {
  // Fake ML service is needed because ml service client library
  // requires the ml service daemon, which is not present in the
  // unit test or browser test environment.
  auto* fake_service_connection =
      new chromeos::machine_learning::FakeServiceConnectionImpl();
  fake_service_connection->Initialize();
  chromeos::machine_learning::ServiceConnection::
      UseFakeServiceConnectionForTesting(fake_service_connection);
}

}  // namespace test
