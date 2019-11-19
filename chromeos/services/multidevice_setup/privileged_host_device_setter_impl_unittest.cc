// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/privileged_host_device_setter_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

class MultiDeviceSetupPrivilegedHostDeviceSetterImplTest
    : public testing::Test {
 protected:
  MultiDeviceSetupPrivilegedHostDeviceSetterImplTest() = default;
  ~MultiDeviceSetupPrivilegedHostDeviceSetterImplTest() override = default;

  void SetUp() override {
    fake_multidevice_setup_ = std::make_unique<FakeMultiDeviceSetup>();
    host_setter_ =
        PrivilegedHostDeviceSetterImpl::Factory::Get()->BuildInstance(
            fake_multidevice_setup_.get());
  }

  void CallSetHostDevice(const std::string& host_device_id,
                         bool should_succeed) {
    auto& args = fake_multidevice_setup_->set_host_without_auth_args();
    size_t num_calls_before = args.size();

    host_setter_->SetHostDevice(
        host_device_id,
        base::BindOnce(&MultiDeviceSetupPrivilegedHostDeviceSetterImplTest::
                           OnSetHostDeviceResult,
                       base::Unretained(this)));

    EXPECT_EQ(num_calls_before + 1u, args.size());
    EXPECT_EQ(host_device_id, args.back().first);
    std::move(args.back().second).Run(should_succeed);
    EXPECT_EQ(should_succeed, *last_set_host_success_);
    last_set_host_success_.reset();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  void OnSetHostDeviceResult(bool success) {
    EXPECT_FALSE(last_set_host_success_);
    last_set_host_success_ = success;
  }

  base::Optional<bool> last_set_host_success_;

  std::unique_ptr<FakeMultiDeviceSetup> fake_multidevice_setup_;
  std::unique_ptr<PrivilegedHostDeviceSetterBase> host_setter_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupPrivilegedHostDeviceSetterImplTest);
};

TEST_F(MultiDeviceSetupPrivilegedHostDeviceSetterImplTest, SetHostDevice) {
  CallSetHostDevice("hostDeviceId1", false /* should_succeed */);
  CallSetHostDevice("hostDeviceId2", true /* should_succeed */);
}

}  // namespace multidevice_setup

}  // namespace chromeos
