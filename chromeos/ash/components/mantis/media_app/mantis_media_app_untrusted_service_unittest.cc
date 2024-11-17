// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_media_app_untrusted_service.h"

#include <cstdint>
#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/mantis/mojom/mantis_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::base::test::RunOnceCallback;
using ::base::test::TestFuture;
using ::mantis::mojom::InitializeResult;
using ::mantis::mojom::MantisFeatureStatus;

class MockMojoMantisService : public mantis::mojom::MantisService {
 public:
  MOCK_METHOD(void,
              GetMantisFeatureStatus,
              (GetMantisFeatureStatusCallback callback),
              (override));
  MOCK_METHOD(void,
              Initialize,
              (mojo::PendingRemote<mantis::mojom::PlatformModelProgressObserver>
                   progress_observer,
               mojo::PendingReceiver<mantis::mojom::MantisProcessor> processor,
               InitializeCallback callback),
              (override));

  mojo::PendingRemote<mantis::mojom::MantisService> GetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mantis::mojom::MantisService> receiver_{this};
};

class UntrustedServiceTest : public testing::Test {
 public:
  UntrustedServiceTest()
      : service_(mojo::NullReceiver(), mojo_mantis_service_.GetRemote()) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  MockMojoMantisService mojo_mantis_service_;
  MantisMediaAppUntrustedService service_;
};

class GetMantisFeatureStatusTest
    : public UntrustedServiceTest,
      public testing::WithParamInterface<MantisFeatureStatus> {};

TEST_P(GetMantisFeatureStatusTest, GetMantisFeatureStatus) {
  const MantisFeatureStatus status = GetParam();

  EXPECT_CALL(mojo_mantis_service_, GetMantisFeatureStatus)
      .WillOnce(RunOnceCallback<0>(status));

  TestFuture<MantisFeatureStatus> result_future;
  service_.GetMantisFeatureStatus(result_future.GetCallback());
  EXPECT_EQ(result_future.Take(), status);
}

INSTANTIATE_TEST_SUITE_P(
    MantisMediaApp,
    GetMantisFeatureStatusTest,
    testing::Values(MantisFeatureStatus::kDeviceNotSupported,
                    MantisFeatureStatus::kARCVMDisabled,
                    MantisFeatureStatus::kAvailable),
    testing::PrintToStringParamName());

class InitializeTest : public UntrustedServiceTest,
                       public testing::WithParamInterface<InitializeResult> {};

TEST_P(InitializeTest, Initialize) {
  const InitializeResult result = GetParam();

  EXPECT_CALL(mojo_mantis_service_, Initialize)
      .WillOnce(RunOnceCallback<2>(result));

  TestFuture<InitializeResult> result_future;
  service_.Initialize(mojo::NullReceiver(), result_future.GetCallback());
  EXPECT_EQ(result_future.Take(), result);
}

INSTANTIATE_TEST_SUITE_P(
    MantisMediaApp,
    InitializeTest,
    testing::Values(InitializeResult::kSuccess,
                    InitializeResult::kGpuBlocked,
                    InitializeResult::kFailedToLoadLibrary),
    testing::PrintToStringParamName());

}  // namespace
}  // namespace ash
