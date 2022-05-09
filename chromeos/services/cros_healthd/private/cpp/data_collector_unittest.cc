// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/private/cpp/data_collector.h"

#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace cros_healthd {
namespace internal {
namespace {

constexpr char kFakeTouchpadLibraryName[] = "FakeTouchpadLibraryName";

class FakeDataCollectorDelegate : public DataCollector::Delegate {
 public:
  FakeDataCollectorDelegate() = default;
  ~FakeDataCollectorDelegate() override = default;

  std::string GetTouchpadLibraryName() override {
    return kFakeTouchpadLibraryName;
  }
};

class DataCollectorTest : public testing::Test {
 protected:
  void SetUp() override {
    DataCollector::InitializeWithDelegateForTesting(&delegate_);
    DataCollector::Get()->BindReceiver(remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    remote_.reset();
    DataCollector::Shutdown();
  }

  // The test environment.
  base::test::TaskEnvironment env_;
  // The mojo remote to the data collector.
  mojo::Remote<mojom::ChromiumDataCollector> remote_;
  // The fake delegate for DataCollector.
  FakeDataCollectorDelegate delegate_;
};

TEST_F(DataCollectorTest, GetTouchscreenDevices) {
  NOTIMPLEMENTED();
}

TEST_F(DataCollectorTest, GetTouchpadLibraryName) {
  base::RunLoop run_loop;
  remote_->GetTouchpadLibraryName(
      base::BindLambdaForTesting([&](const std::string& library_name) {
        EXPECT_EQ(library_name, kFakeTouchpadLibraryName);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace internal
}  // namespace cros_healthd
}  // namespace chromeos
