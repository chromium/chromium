// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/privileged_host_device_setter_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace multidevice_setup {

class MultiDeviceSetupPrivilegedHostDeviceSetterImplTest
    : public testing::Test {
 public:
  MultiDeviceSetupPrivilegedHostDeviceSetterImplTest(
      const MultiDeviceSetupPrivilegedHostDeviceSetterImplTest&) = delete;
  MultiDeviceSetupPrivilegedHostDeviceSetterImplTest& operator=(
      const MultiDeviceSetupPrivilegedHostDeviceSetterImplTest&) = delete;

 protected:
  MultiDeviceSetupPrivilegedHostDeviceSetterImplTest() = default;
  ~MultiDeviceSetupPrivilegedHostDeviceSetterImplTest() override = default;

  void SetUp() override {
    fake_multidevice_setup_ = std::make_unique<FakeMultiDeviceSetup>();
    host_setter_ = PrivilegedHostDeviceSetterImpl::Factory::Create(
        fake_multidevice_setup_.get());
  }

  void CallSetHostDevice(
      const std::string& host_instance_id_or_legacy_device_id,
      bool should_succeed) {
    auto& args = fake_multidevice_setup_->set_host_without_auth_args();
    size_t num_calls_before = args.size();

    host_setter_->SetHostDevice(
        host_instance_id_or_legacy_device_id,
        base::BindOnce(&MultiDeviceSetupPrivilegedHostDeviceSetterImplTest::
                           OnSetHostDeviceResult,
                       base::Unretained(this)));

    EXPECT_EQ(num_calls_before + 1u, args.size());
    EXPECT_EQ(host_instance_id_or_legacy_device_id, args.back().first);
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

  std::optional<bool> last_set_host_success_;

  std::unique_ptr<FakeMultiDeviceSetup> fake_multidevice_setup_;
  std::unique_ptr<PrivilegedHostDeviceSetterBase> host_setter_;
};

TEST_F(MultiDeviceSetupPrivilegedHostDeviceSetterImplTest, SetHostDevice) {
  CallSetHostDevice("hostId1", false /* should_succeed */);
  CallSetHostDevice("hostId2", true /* should_succeed */);
}

}  // namespace multidevice_setup

}  // namespace ash
