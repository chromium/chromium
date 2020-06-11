// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/feedback/cpp/fidl.h>
#include <fuchsia/feedback/cpp/fidl_test_base.h>
#include <fuchsia/io/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>
#include <memory>
#include <tuple>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "chromecast/public/reboot_shlib.h"
#include "chromecast/system/reboot/reboot_fuchsia.h"
#include "chromecast/system/reboot/reboot_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

using ::testing::Eq;

using fuchsia::feedback::RebootReason;

struct RebootReasonParam {
  RebootReason reason;
  RebootShlib::RebootSource source;
  bool graceful;
};

const RebootReasonParam kRebootReasonParams[] = {
    {RebootReason::COLD, RebootShlib::RebootSource::FORCED, false},
    {RebootReason::BRIEF_POWER_LOSS, RebootShlib::RebootSource::FORCED, false},
    {RebootReason::BROWNOUT, RebootShlib::RebootSource::FORCED, false},
    {RebootReason::KERNEL_PANIC, RebootShlib::RebootSource::FORCED, false},
    {RebootReason::SYSTEM_OUT_OF_MEMORY,
     RebootShlib::RebootSource::REPEATED_OOM, false},
    {RebootReason::HARDWARE_WATCHDOG_TIMEOUT,
     RebootShlib::RebootSource::HW_WATCHDOG, false},
    {RebootReason::HARDWARE_WATCHDOG_TIMEOUT,
     RebootShlib::RebootSource::HW_WATCHDOG, false},
    {RebootReason::SOFTWARE_WATCHDOG_TIMEOUT,
     RebootShlib::RebootSource::WATCHDOG, false},
};

class FakeLastRebootInfoProvider
    : public fuchsia::feedback::testing::LastRebootInfoProvider_TestBase {
 public:
  explicit FakeLastRebootInfoProvider(
      fidl::InterfaceRequest<fuchsia::io::Directory> channel) {
    outgoing_directory_.GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OPEN_RIGHT_READABLE | fuchsia::io::OPEN_RIGHT_WRITABLE,
        channel.TakeChannel());
    binding_ = std::make_unique<base::fuchsia::ScopedServiceBinding<
        fuchsia::feedback::LastRebootInfoProvider>>(&outgoing_directory_, this);
  }

  void SetLastReboot(fuchsia::feedback::LastReboot last_reboot) {
    last_reboot_ = std::move(last_reboot);
  }

  void Get(GetCallback callback) override { callback(std::move(last_reboot_)); }

  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "NotImplemented_: " << name;
  }

 private:
  sys::OutgoingDirectory outgoing_directory_;
  std::unique_ptr<base::fuchsia::ScopedServiceBinding<
      fuchsia::feedback::LastRebootInfoProvider>>
      binding_;
  fuchsia::feedback::LastReboot last_reboot_;
};

class RebootFuchsiaTest : public ::testing::TestWithParam<RebootReasonParam> {
 public:
  RebootFuchsiaTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        thread_("FakeLastRebootInfoProvider_Thread") {
    CHECK(thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0)));
  }

  void SetUp() override {
    // Create incoming (service) and outgoing directories that are connected.
    fidl::InterfaceHandle<::fuchsia::io::Directory> directory;
    last_reboot_info_provider_ =
        base::SequenceBound<FakeLastRebootInfoProvider>(thread_.task_runner(),
                                                        directory.NewRequest());
    incoming_directory_ =
        std::make_unique<sys::ServiceDirectory>(std::move(directory));
    InitializeRebootShlib({}, incoming_directory_.get());
  }

  void TearDown() override { RebootUtil::Finalize(); }

  void SetLastReboot(fuchsia::feedback::LastReboot last_reboot) {
    last_reboot_info_provider_.Post(FROM_HERE,
                                    &FakeLastRebootInfoProvider::SetLastReboot,
                                    std::move(last_reboot));
  }

 private:
  const base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<sys::ServiceDirectory> incoming_directory_;
  base::Thread thread_;
  base::SequenceBound<FakeLastRebootInfoProvider> last_reboot_info_provider_;
};

TEST_F(RebootFuchsiaTest, GetLastRebootSourceDefaultsToUnknown) {
  EXPECT_THAT(RebootUtil::GetLastRebootSource(),
              Eq(RebootShlib::RebootSource::UNKNOWN));
}

TEST_F(RebootFuchsiaTest, GetLastRebootSourceWithoutGranularReason) {
  fuchsia::feedback::LastReboot last_reboot;
  last_reboot.set_graceful(true);
  EXPECT_TRUE(last_reboot.has_graceful());
  EXPECT_FALSE(last_reboot.has_reason());
  SetLastReboot(std::move(last_reboot));
  EXPECT_THAT(RebootUtil::GetLastRebootSource(),
              Eq(RebootShlib::RebootSource::SW_OTHER));
}

TEST_P(RebootFuchsiaTest, GetLastRebootSourceTranslatesReasonFromFuchsia) {
  fuchsia::feedback::LastReboot last_reboot;
  last_reboot.set_graceful(GetParam().graceful);
  last_reboot.set_reason(GetParam().reason);
  EXPECT_TRUE(last_reboot.has_graceful());
  EXPECT_TRUE(last_reboot.has_reason());
  SetLastReboot(std::move(last_reboot));
  EXPECT_THAT(RebootUtil::GetLastRebootSource(), Eq(GetParam().source));
}

INSTANTIATE_TEST_SUITE_P(RebootReasonParamSweep,
                         RebootFuchsiaTest,
                         ::testing::ValuesIn(kRebootReasonParams));

}  // namespace
}  // namespace chromecast
