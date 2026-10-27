// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/device_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/test/chromedriver/chrome/adb.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeAdb : public Adb {
 public:
  FakeAdb() = default;
  ~FakeAdb() override = default;

  Status GetDevices(std::vector<std::string>* devices) override {
    devices->push_back("a");
    devices->push_back("b");
    return Status(kOk);
  }

  Status ForwardPort(const std::string& device_serial,
                     const std::string& remote_abstract,
                     int* local_port) override {
    return Status(kOk);
  }

  Status KillForwardPort(const std::string& device_serial,
                         int port) override {
    return Status(kOk);
  }

  Status SetCommandLineFile(const std::string& device_serial,
                            const std::string& command_line_file,
                            const std::string& exec_name,
                            const std::string& args) override {
    return Status(kOk);
  }

  Status CheckAppInstalled(const std::string& device_serial,
                           const std::string& package) override {
    return Status(kOk);
  }

  Status ClearAppData(const std::string& device_serial,
                      const std::string& package) override {
    return Status(kOk);
  }

  Status GrantPermission(const std::string& device_serial,
                         const std::string& package,
                         const std::string& permission) override {
    return Status(kOk);
  }

  Status SetDebugApp(const std::string& device_serial,
                     const std::string& package) override {
    return Status(kOk);
  }

  Status Launch(const std::string& device_serial,
                const std::string& package,
                const std::string& activity) override {
    return Status(kOk);
  }

  Status ForceStop(const std::string& device_serial,
                   const std::string& package) override {
    return Status(kOk);
  }

  Status GetPidByName(const std::string& device_serial,
                      const std::string& process_name,
                      int* pid) override {
    *pid = 0;  // avoid uninit error crbug.com/41119796
    return Status(kOk);
  }

  Status GetSocketByPattern(const std::string& device_serial,
                            const std::string& grep_pattern,
                            std::string* socket_name) override {
    *socket_name = "@webview_devtools_remote_0";
    return Status(kOk);
  }

  Status SetPreferences(const std::string& device_serial,
                        const std::string& path,
                        const base::DictValue* custom_prefs) override {
    return Status(kOk);
  }

  Status SetLocalState(const std::string& device_serial,
                       const std::string& path,
                       const base::DictValue* custom_local_state) override {
    return Status(kOk);
  }
};

class ClearAppDataCalledFakeAdb : public FakeAdb {
 public:
  Status ClearAppData(const std::string& device_serial,
                      const std::string& package) override {
    clear_app_data_called = true;
    return FakeAdb::ClearAppData(device_serial, package);
  }

  bool ClearAppDataCalledIsCalled() { return clear_app_data_called; }

 private:
  bool clear_app_data_called = false;
};

// Records the order of the app setup calls made on the device.
class RecordingFakeAdb : public FakeAdb {
 public:
  Status ClearAppData(const std::string& device_serial,
                      const std::string& package) override {
    calls.push_back("clear");
    return FakeAdb::ClearAppData(device_serial, package);
  }

  Status GrantPermission(const std::string& device_serial,
                         const std::string& package,
                         const std::string& permission) override {
    calls.push_back("grant " + permission);
    if (fail_grants) {
      return Status(kUnknownError, "cannot grant " + permission);
    }
    return FakeAdb::GrantPermission(device_serial, package, permission);
  }

  Status Launch(const std::string& device_serial,
                const std::string& package,
                const std::string& activity) override {
    calls.push_back("launch");
    return FakeAdb::Launch(device_serial, package, activity);
  }

  std::vector<std::string> calls;
  bool fail_grants = false;
};

class SucceedsForwardPortFakeAdb : public FakeAdb {
 public:
  SucceedsForwardPortFakeAdb() = default;
  ~SucceedsForwardPortFakeAdb() override = default;

  Status ForwardPort(const std::string& device_serial,
                     const std::string& remote_abstract,
                     int* local_port) override {
    *local_port = 1;
    return Status(kOk);
  }

  Status KillForwardPort(const std::string& device_serial,
                         int port) override {
    kill_forward_port_is_called_ = true;
    return Status(kOk);
  }

  bool KillForwardPortIsCalled() {
    return kill_forward_port_is_called_;
  }
 private:
  bool kill_forward_port_is_called_ = false;
};

class FailsForwardPortFakeAdb : public SucceedsForwardPortFakeAdb {
 public:
  FailsForwardPortFakeAdb() = default;
  ~FailsForwardPortFakeAdb() override = default;

  Status ForwardPort(const std::string& device_serial,
                     const std::string& remote_abstract,
                     int* local_port) override {
    *local_port = 1;
    return Status(kUnknownError);
  }
};


}  // namespace

TEST(DeviceManager, AcquireDevice) {
  FakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device1;
  std::unique_ptr<Device> device2;
  std::unique_ptr<Device> device3;
  ASSERT_TRUE(device_manager.AcquireDevice(&device1).IsOk());
  ASSERT_TRUE(device_manager.AcquireDevice(&device2).IsOk());
  ASSERT_FALSE(device_manager.AcquireDevice(&device3).IsOk());
  device1.reset();
  ASSERT_TRUE(device_manager.AcquireDevice(&device3).IsOk());
  ASSERT_FALSE(device_manager.AcquireDevice(&device1).IsOk());
}

TEST(DeviceManager, AcquireSpecificDevice) {
  FakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device1;
  std::unique_ptr<Device> device2;
  std::unique_ptr<Device> device3;
  ASSERT_TRUE(device_manager.AcquireSpecificDevice("a", &device1).IsOk());
  ASSERT_FALSE(device_manager.AcquireSpecificDevice("a", &device2).IsOk());
  ASSERT_TRUE(device_manager.AcquireSpecificDevice("b", &device3).IsOk());
  device1.reset();
  ASSERT_TRUE(device_manager.AcquireSpecificDevice("a", &device2).IsOk());
  ASSERT_FALSE(device_manager.AcquireSpecificDevice("a", &device1).IsOk());
  ASSERT_FALSE(device_manager.AcquireSpecificDevice("b", &device1).IsOk());
}

TEST(Device, StartStopApp) {
  int devtools_port = 0;
  FakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device1;
  ASSERT_TRUE(device_manager.AcquireDevice(&device1).IsOk());
  ASSERT_TRUE(device1->TearDown().IsOk());
  ASSERT_TRUE(device1
                  ->SetUp("a.chrome.package", "", "", "", "", "", false, false,
                          std::nullopt, &devtools_port)
                  .IsOk());
  ASSERT_FALSE(device1
                   ->SetUp("a.chrome.package", "", "", "", "", "", false, false,
                           std::nullopt, &devtools_port)
                   .IsOk());
  ASSERT_TRUE(device1->TearDown().IsOk());
  ASSERT_FALSE(device1
                   ->SetUp("a.chrome.package", "an.activity", "", "", "", "",
                           false, false, std::nullopt, &devtools_port)
                   .IsOk());
  ASSERT_FALSE(device1
                   ->SetUp("a.package", "", "", "", "", "", false, false,
                           std::nullopt, &devtools_port)
                   .IsOk());
  ASSERT_TRUE(device1
                  ->SetUp("a.package", "an.activity", "", "", "", "", false,
                          false, std::nullopt, &devtools_port)
                  .IsOk());
  ASSERT_TRUE(device1->TearDown().IsOk());
  ASSERT_TRUE(device1
                  ->SetUp("a.package", "an.activity", "a.process", "", "", "",
                          false, false, std::nullopt, &devtools_port)
                  .IsOk());
  ASSERT_TRUE(device1->TearDown().IsOk());
  ASSERT_TRUE(device1
                  ->SetUp("a.package", "an.activity", "a.process",
                          "a.deviceSocket", "", "", false, false, std::nullopt,
                          &devtools_port)
                  .IsOk());
  ASSERT_TRUE(device1->TearDown().IsOk());
  ASSERT_TRUE(device1
                  ->SetUp("a.package", "an.activity", "a.process",
                          "a.deviceSocket", "an.execName", "", false, false,
                          std::nullopt, &devtools_port)
                  .IsOk());
  ASSERT_TRUE(device1->TearDown().IsOk());
}

TEST(Device, ClearAppDataCalled) {
  int devtools_port;
  ClearAppDataCalledFakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device1;
  ASSERT_TRUE(device_manager.AcquireDevice(&device1).IsOk());
  ASSERT_TRUE(device1
                  ->SetUp("a.chrome.package", "", "", "", "", "", false, false,
                          std::nullopt, &devtools_port)
                  .IsOk());
  ASSERT_TRUE(adb.ClearAppDataCalledIsCalled());
}

TEST(Device, ClearAppDataNotCalled) {
  int devtools_port;
  ClearAppDataCalledFakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device1;
  ASSERT_TRUE(device_manager.AcquireDevice(&device1).IsOk());
  ASSERT_TRUE(device1
                  ->SetUp("a.chrome.package", "", "", "", "", "", false, true,
                          std::nullopt, &devtools_port)
                  .IsOk());
  ASSERT_FALSE(adb.ClearAppDataCalledIsCalled());
}

TEST(ForwardPort, Success) {
  int devtools_port;
  SucceedsForwardPortFakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device1;
  device_manager.AcquireDevice(&device1);
  device1->SetUp("a.chrome.package", "", "", "", "", "", false, false,
                 std::nullopt, &devtools_port);
  device1->TearDown();
  ASSERT_TRUE(adb.KillForwardPortIsCalled());
}

TEST(ForwardPort, Failure) {
  int devtools_port;
  FailsForwardPortFakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device1;
  device_manager.AcquireDevice(&device1);
  device1->SetUp("a.package", "an.activity", "", "", "", "", false, false,
                 std::nullopt, &devtools_port);
  device1->TearDown();
  ASSERT_FALSE(adb.KillForwardPortIsCalled());
}

TEST(Device, GrantsDefaultPermissionsBetweenClearAndLaunch) {
  int devtools_port;
  RecordingFakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device;
  ASSERT_TRUE(device_manager.AcquireDevice(&device).IsOk());
  ASSERT_TRUE(device
                  ->SetUp("a.chrome.package", "", "", "", "", "", false, false,
                          std::nullopt, &devtools_port)
                  .IsOk());
  EXPECT_EQ(
      (std::vector<std::string>{
          "clear", "grant android.permission.POST_NOTIFICATIONS", "launch"}),
      adb.calls);
}

TEST(Device, GrantsExplicitPermissionsInsteadOfDefault) {
  int devtools_port;
  RecordingFakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device;
  ASSERT_TRUE(device_manager.AcquireDevice(&device).IsOk());
  std::vector<std::string> permissions = {
      "android.permission.ACCESS_LOCAL_NETWORK", "com.example.permission.FOO"};
  ASSERT_TRUE(device
                  ->SetUp("a.chrome.package", "", "", "", "", "", false, false,
                          permissions, &devtools_port)
                  .IsOk());
  EXPECT_EQ((std::vector<std::string>{
                "clear", "grant android.permission.ACCESS_LOCAL_NETWORK",
                "grant com.example.permission.FOO", "launch"}),
            adb.calls);
}

TEST(Device, GrantsNothingForEmptyPermissionList) {
  int devtools_port;
  RecordingFakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device;
  ASSERT_TRUE(device_manager.AcquireDevice(&device).IsOk());
  ASSERT_TRUE(device
                  ->SetUp("a.chrome.package", "", "", "", "", "", false, false,
                          std::vector<std::string>(), &devtools_port)
                  .IsOk());
  EXPECT_EQ((std::vector<std::string>{"clear", "launch"}), adb.calls);
}

TEST(Device, ExplicitPermissionGrantFailureFailsSetUp) {
  int devtools_port;
  RecordingFakeAdb adb;
  adb.fail_grants = true;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device;
  ASSERT_TRUE(device_manager.AcquireDevice(&device).IsOk());
  std::vector<std::string> permissions = {
      "android.permission.ACCESS_LOCAL_NETWORK"};
  ASSERT_TRUE(device
                  ->SetUp("a.chrome.package", "", "", "", "", "", false, false,
                          permissions, &devtools_port)
                  .IsError());
  EXPECT_EQ((std::vector<std::string>{
                "clear", "grant android.permission.ACCESS_LOCAL_NETWORK"}),
            adb.calls);
}

TEST(Device, DefaultPermissionGrantFailureIsIgnored) {
  int devtools_port;
  RecordingFakeAdb adb;
  adb.fail_grants = true;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device;
  ASSERT_TRUE(device_manager.AcquireDevice(&device).IsOk());
  ASSERT_TRUE(device
                  ->SetUp("a.chrome.package", "", "", "", "", "", false, false,
                          std::nullopt, &devtools_port)
                  .IsOk());
  ASSERT_FALSE(adb.calls.empty());
  EXPECT_EQ("launch", adb.calls.back());
}

TEST(Device, GrantsPermissionsWhenKeepingAppData) {
  int devtools_port;
  RecordingFakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device;
  ASSERT_TRUE(device_manager.AcquireDevice(&device).IsOk());
  std::vector<std::string> permissions = {
      "android.permission.ACCESS_LOCAL_NETWORK"};
  ASSERT_TRUE(device
                  ->SetUp("a.chrome.package", "", "", "", "", "", false, true,
                          permissions, &devtools_port)
                  .IsOk());
  EXPECT_EQ((std::vector<std::string>{
                "grant android.permission.ACCESS_LOCAL_NETWORK", "launch"}),
            adb.calls);
}

TEST(Device, NoPermissionGrantsWhenUsingRunningApp) {
  int devtools_port;
  RecordingFakeAdb adb;
  DeviceManager device_manager(&adb);
  std::unique_ptr<Device> device;
  ASSERT_TRUE(device_manager.AcquireDevice(&device).IsOk());
  std::vector<std::string> permissions = {
      "android.permission.ACCESS_LOCAL_NETWORK"};
  ASSERT_TRUE(device
                  ->SetUp("a.chrome.package", "", "", "", "", "", true, false,
                          permissions, &devtools_port)
                  .IsOk());
  EXPECT_TRUE(adb.calls.empty());
}
