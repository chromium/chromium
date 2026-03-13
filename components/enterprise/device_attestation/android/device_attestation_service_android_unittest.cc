// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/android/device_attestation_service_android.h"

#include <sys/mman.h>

#include <string>
#include <string_view>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise {

class DeviceAttestationServiceAndroidTest : public testing::Test {
 public:
  DeviceAttestationServiceAndroidTest() = default;
  ~DeviceAttestationServiceAndroidTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  DeviceAttestationServiceAndroid device_attestation_service_android_;
};

// Tests that GetAttestationResponse safely copies `std::string_view` arguments,
// preventing a use-after-free crash when the original strings are destroyed
// before the background task executes.
TEST_F(DeviceAttestationServiceAndroidTest,
       GetAttestationResponse_DoesNotCrashOnTemporaryStrings) {
  base::RunLoop run_loop;

  // Explicitly construct and pass a large temporary std::string object for
  // request payload only, to avoid Small String Optimization. This ensures it
  // to be allocated on the heap and destroyed at the end of this statement.
  device_attestation_service_android_.GetAttestationResponse(
      std::string("flow_name"),
      // Make the payload large enough to guarantee a deterministic crash by
      // forcing allocator unmapping.
      std::string_view(std::string(1024 * 1024, 'r')), std::string("timestamp"),
      std::string("nonce"),
      base::BindLambdaForTesting([&](const BlobGenerationResult& result) {
        // We only care that the thread pool task didn't memory-crash above.
        run_loop.Quit();
      }));

  run_loop.Run();
}

}  // namespace enterprise
