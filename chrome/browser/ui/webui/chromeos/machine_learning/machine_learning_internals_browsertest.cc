// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_browsertest.h"

#include <vector>

#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

MachineLearningInternalsBrowserTest::MachineLearningInternalsBrowserTest() = default;
MachineLearningInternalsBrowserTest::~MachineLearningInternalsBrowserTest() = default;

void MachineLearningInternalsBrowserTest::SetupFakeConnectionAndOutput(
    double fake_output) {
  auto* fake_client =
      new chromeos::machine_learning::FakeServiceConnectionImpl();
  fake_client->SetOutputValue(std::vector<int64_t>{1L},
                              std::vector<double>{fake_output});
  chromeos::machine_learning::ServiceConnection
      ::UseFakeServiceConnectionForTesting(fake_client);
}
