// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/feedback/cpp/fidl.h>
#include <fuchsia/feedback/cpp/fidl_test_base.h>
#include <fuchsia/hardware/power/statecontrol/cpp/fidl.h>
#include <fuchsia/hardware/power/statecontrol/cpp/fidl_test_base.h>
#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/recovery/cpp/fidl.h>
#include <fuchsia/recovery/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/fpromise/result.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>

#include <memory>
#include <string_view>
#include <tuple>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
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
using ::testing::Ne;

using fuchsia::feedback::RebootReason;
using StateControlRebootReason =
    fuchsia::hardware::power::statecontrol::RebootReason;

struct RebootReasonParam {
  RebootReason reason;
  RebootShlib::RebootSource source;
  bool graceful;
  StateControlRebootReason state_control_reason =
      StateControlRebootReason::USER_REQUEST;
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

    // Graceful reboot reasons.
    {RebootReason::USER_REQUEST, RebootShlib::RebootSource::API, true,
     StateControlRebootReason::USER_REQUEST},
    {RebootReason::SYSTEM_UPDATE, RebootShlib::RebootSource::OTA, true,
     StateControlRebootReason::SYSTEM_UPDATE},
    {RebootReason::HIGH_TEMPERATURE, RebootShlib::RebootSource::OVERHEAT, true,
     StateControlRebootReason::HIGH_TEMPERATURE},
    {RebootReason::SESSION_FAILURE, RebootShlib::RebootSource::SW_OTHER, true},
};

constexpr char kStartedOnce[] = "component-started-once";
constexpr char kGracefulTeardown[] = "component-graceful-teardown";

struct RestartReasonParam {
  RebootShlib::RebootSource source;
  bool graceful;
  const char* file;
};

const RestartReasonParam kRestartReasonParams[] = {
    {RebootShlib::RebootSource::UNGRACEFUL_RESTART, false, kStartedOnce},
    {RebootShlib::RebootSource::GRACEFUL_RESTART, true, kGracefulTeardown},
};

class FakeAdmin
    : public fuchsia::hardware::power::statecontrol::testing::Admin_TestBase {
 public:
  explicit FakeAdmin(sys::OutgoingDirectory* outgoing_directory)
      : binding_(outgoing_directory, this) {}

  void GetLastRebootReason(StateControlRebootReason* reason) {
    *reason = last_reboot_reason_;
  }

 private:
  void Reboot(StateControlRebootReason reason, RebootCallback callback) final {
    last_reboot_reason_ = reason;
    callback(fpromise::ok());
  }

  void NotImplemented_(const std::string& name) final {
    ADD_FAILURE() << "NotImplemented_: " << name;
  }

  base::ScopedServiceBinding<fuchsia::hardware::power::statecontrol::Admin>
      binding_;
  StateControlRebootReason last_reboot_reason_;
};

class FakeLastRebootInfoProvider
    : public fuchsia::feedback::testing::LastRebootInfoProvider_TestBase {
 public:
  explicit FakeLastRebootInfoProvider(
      sys::OutgoingDirectory* outgoing_directory)
      : binding_(outgoing_directory, this) {}

  void SetLastReboot(fuchsia::feedback::LastReboot last_reboot) {
    last_reboot_ = std::move(last_reboot);
  }

 private:
  void Get(GetCallback callback) final { callback(std::move(last_reboot_)); }

  void NotImplemented_(const std::string& name) final {
    ADD_FAILURE() << "NotImplemented_: " << name;
  }

  base::ScopedServiceBinding<fuchsia::feedback::LastRebootInfoProvider>
      binding_;
  fuchsia::feedback::LastReboot last_reboot_;
};

class FakeFactoryReset
    : public fuchsia::recovery::testing::FactoryReset_TestBase {
 public:
  explicit FakeFactoryReset(sys::OutgoingDirectory* outgoing_directory)
      : binding_(outgoing_directory, this) {}

  void reset_called(bool* reset_called) { *reset_called = reset_called_; }

 private:
  void Reset(ResetCallback callback) final {
    reset_called_ = true;
    callback(ZX_OK);
  }

  void NotImplemented_(const std::string& name) final {
    ADD_FAILURE() << "NotImplemented_: " << name;
  }

  base::ScopedServiceBinding<fuchsia::recovery::FactoryReset> binding_;
  bool reset_called_ = false;
};

class RebootFuchsiaTest : public ::testing::Test {
 public:
  RebootFuchsiaTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        thread_("FakeLastRebootInfoProvider_Thread") {
    CHECK(thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0)));
  }

  void SetUp() override {
    // Create incoming (service) and outgoing directories that are connected.
    fidl::InterfaceHandle<fuchsia::io::Directory> directory;

    // The thread handling fidl calls to the fake service must also be the
    // thread that we start the serve operation on. Since all fakes require the
    // same output directory handle, we post a task here to begin the serve
    // operation, then flush the task runner queue to ensure that output
    // directory is safe to pass to the fakes.
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RebootFuchsiaTest::ServeOutgoingDirectory,
                       base::Unretained(this), directory.NewRequest()));
    thread_.FlushForTesting();

    // Initialize and publish fake fidl services.
    admin_ = base::SequenceBound<FakeAdmin>(thread_.task_runner(),
                                            outgoing_directory_.get());
    last_reboot_info_provider_ =
        base::SequenceBound<FakeLastRebootInfoProvider>(
            thread_.task_runner(), outgoing_directory_.get());
    factory_reset_service_ = base::SequenceBound<FakeFactoryReset>(
        thread_.task_runner(), outgoing_directory_.get());

    // Ensure that the services above finish publishing themselves.
    thread_.FlushForTesting();

    // Use a service directory backed by the fakes above for tests.
    incoming_directory_ =
        std::make_unique<sys::ServiceDirectory>(std::move(directory));
    InitializeRebootShlib({}, incoming_directory_.get());
    EXPECT_TRUE(dir_.CreateUniqueTempDir());
    full_path_ = InitializeFlagFileDirForTesting(dir_.GetPath());
  }

  StateControlRebootReason GetLastRebootReason() {
    StateControlRebootReason reason;
    admin_.AsyncCall(&FakeAdmin::GetLastRebootReason).WithArgs(&reason);
    thread_.FlushForTesting();
    return reason;
  }

  void SetLastReboot(fuchsia::feedback::LastReboot last_reboot) {
    last_reboot_info_provider_
        .AsyncCall(&FakeLastRebootInfoProvider::SetLastReboot)
        .WithArgs(std::move(last_reboot));
    thread_.FlushForTesting();
  }

  bool FdrTriggered() {
    bool reset_called;
    factory_reset_service_.AsyncCall(&FakeFactoryReset::reset_called)
        .WithArgs(&reset_called);
    thread_.FlushForTesting();
    return reset_called;
  }

 private:
  void ServeOutgoingDirectory(
      fidl::InterfaceRequest<fuchsia::io::Directory> channel) {
    outgoing_directory_ = std::make_unique<sys::OutgoingDirectory>();
    outgoing_directory_->GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OpenFlags::RIGHT_READABLE |
            fuchsia::io::OpenFlags::RIGHT_WRITABLE,
        channel.TakeChannel());
  }

  const base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<sys::OutgoingDirectory> outgoing_directory_;
  std::unique_ptr<sys::ServiceDirectory> incoming_directory_;
  base::SequenceBound<FakeAdmin> admin_;
  base::SequenceBound<FakeLastRebootInfoProvider> last_reboot_info_provider_;
  base::SequenceBound<FakeFactoryReset> factory_reset_service_;
  base::ScopedTempDir dir_;
  base::FilePath full_path_;

 protected:
  base::FilePath GenerateFlagFilePath(std::string_view name) {
    return full_path_.Append(name);
  }

  base::Thread thread_;
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

fuchsia::feedback::LastReboot GenerateLastReboot(bool graceful,
                                                 RebootReason reason) {
  fuchsia::feedback::LastReboot last_reboot;
  last_reboot.set_graceful(graceful);
  last_reboot.set_reason(reason);
  return last_reboot;
}

// RetrySystemUpdate must be handled separately because it does not work with
// the RebootFuchsiaParamTest family of tests. Those tests expect
// RebootSource::OTA to map to exactly one StateControlRebootReason, which is
// now not the case.
TEST_F(RebootFuchsiaTest, RebootReasonRetrySystemUpdateTranslatesFromFuchsia) {
  SetLastReboot(GenerateLastReboot(true, RebootReason::RETRY_SYSTEM_UPDATE));
  EXPECT_THAT(RebootUtil::GetLastRebootSource(),
              Eq(RebootShlib::RebootSource::OTA));
}

TEST_F(RebootFuchsiaTest, RebootReasonZbiSwapTranslatesFromFuchsia) {
  SetLastReboot(GenerateLastReboot(true, RebootReason::ZBI_SWAP));
  EXPECT_THAT(RebootUtil::GetLastRebootSource(),
              Eq(RebootShlib::RebootSource::OTA));
}

TEST_F(RebootFuchsiaTest, RebootNowTriggersFdr) {
  EXPECT_TRUE(RebootShlib::IsFdrForNextRebootSupported());

  RebootShlib::SetFdrForNextReboot();

  EXPECT_TRUE(RebootShlib::RebootNow(RebootShlib::RebootSource::API));
  EXPECT_TRUE(FdrTriggered());
}

class RebootFuchsiaParamTest
    : public RebootFuchsiaTest,
      public ::testing::WithParamInterface<RebootReasonParam> {
 public:
  RebootFuchsiaParamTest() = default;
};

TEST_P(RebootFuchsiaParamTest, RebootNowSendsFidlRebootReason) {
  EXPECT_TRUE(RebootShlib::RebootNow(GetParam().source));
  thread_.FlushForTesting();
  EXPECT_THAT(GetLastRebootReason(), Eq(GetParam().state_control_reason));
}

TEST_P(RebootFuchsiaParamTest, GetLastRebootSourceTranslatesReasonFromFuchsia) {
  SetLastReboot(GenerateLastReboot(GetParam().graceful, GetParam().reason));
  EXPECT_THAT(RebootUtil::GetLastRebootSource(), Eq(GetParam().source));
}

INSTANTIATE_TEST_SUITE_P(RebootReasonParamSweep,
                         RebootFuchsiaParamTest,
                         ::testing::ValuesIn(kRebootReasonParams));

class RestartFuchsiaParamTest
    : public RebootFuchsiaTest,
      public ::testing::WithParamInterface<RestartReasonParam> {
 public:
  RestartFuchsiaParamTest() = default;

  void SetUp() override {
    RebootFuchsiaTest::SetUp();
    base::WriteFile(GenerateFlagFilePath(GetParam().file), "");
  }
};

TEST_P(RestartFuchsiaParamTest, GetLastRestartReasons) {
  fuchsia::feedback::LastReboot last_reboot;
  last_reboot.set_graceful(true);
  EXPECT_TRUE(last_reboot.has_graceful());
  EXPECT_FALSE(last_reboot.has_reason());
  SetLastReboot(std::move(last_reboot));

  EXPECT_THAT(RebootUtil::GetLastRebootSource(), Eq(GetParam().source));

  EXPECT_TRUE(base::PathExists(GenerateFlagFilePath(kStartedOnce)));
  EXPECT_FALSE(base::PathExists(GenerateFlagFilePath(kGracefulTeardown)));

  base::WriteFile(GenerateFlagFilePath(kGracefulTeardown), "");
  EXPECT_THAT(RebootUtil::GetLastRebootSource(), Eq(GetParam().source));
}

INSTANTIATE_TEST_SUITE_P(RestartReasonParamSweep,
                         RestartFuchsiaParamTest,
                         ::testing::ValuesIn(kRestartReasonParams));

TEST_F(RebootFuchsiaTest, ThoroughTestLastRestartReason) {
  fuchsia::feedback::LastReboot last_reboot;
  last_reboot.set_graceful(true);
  EXPECT_TRUE(last_reboot.has_graceful());
  EXPECT_FALSE(last_reboot.has_reason());
  SetLastReboot(std::move(last_reboot));

  EXPECT_FALSE(base::PathExists(GenerateFlagFilePath(kStartedOnce)));
  EXPECT_FALSE(base::PathExists(GenerateFlagFilePath(kGracefulTeardown)));
  EXPECT_THAT(RebootUtil::GetLastRebootSource(),
              Ne(RebootShlib::RebootSource::GRACEFUL_RESTART));
  EXPECT_THAT(RebootUtil::GetLastRebootSource(),
              Ne(RebootShlib::RebootSource::UNGRACEFUL_RESTART));

  // Check files are created/deleted as expected
  const auto once = GenerateFlagFilePath(kStartedOnce);
  LOG(INFO) << "looking at file " << once << " " << base::PathExists(once);
  EXPECT_TRUE(base::PathExists(once));
  EXPECT_FALSE(base::PathExists(GenerateFlagFilePath(kGracefulTeardown)));

  // Confirm reboot reason will not change after create files when check again
  base::WriteFile(GenerateFlagFilePath(kStartedOnce), "");
  base::WriteFile(GenerateFlagFilePath(kGracefulTeardown), "");
  EXPECT_THAT(RebootUtil::GetLastRebootSource(),
              Ne(RebootShlib::RebootSource::GRACEFUL_RESTART));
  EXPECT_THAT(RebootUtil::GetLastRebootSource(),
              Ne(RebootShlib::RebootSource::UNGRACEFUL_RESTART));

  // Emulate Reboot
  RebootUtil::Finalize();
  InitializeRestartCheck();
  EXPECT_THAT(RebootUtil::GetLastRebootSource(),
              Eq(RebootShlib::RebootSource::GRACEFUL_RESTART));
}

}  // namespace
}  // namespace chromecast
