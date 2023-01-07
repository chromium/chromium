// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/device_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/test/chromedriver/chrome/adb.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeAdb : public Adb {
 public:
  FakeAdb() {}
  ~FakeAdb() override {}

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
    *pid = 0;  // avoid uninit error crbug.com/393231
    return Status(kOk);
  }

  Status GetSocketByPattern(const std::string& device_serial,
                            const std::string& grep_pattern,
                            std::string* socket_name) override {
    *socket_name = "@webview_devtools_remote_0";
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

class SucceedsForwardPortFakeAdb : public FakeAdb {
 public:
  SucceedsForwardPortFakeAdb() {}
  ~SucceedsForwardPortFakeAdb() override {}

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
  FailsForwardPortFakeAdb() {}
  ~FailsForwardPortFakeAdb() override {}

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
                          &devtools_port)
                  .IsOk());
  ASSERT_FALSE(device1
                   ->SetUp("a.chrome.package", "", "", "", "", "", false, false,
                           &devtools_port)
                   .IsOk());
  ASSERT_TRUE(device1->TearDown().IsOk());
  ASSERT_FALSE(device1
                   ->SetUp("a.chrome.package", "an.activity", "", "", "", "",
                           false, false, &devtools_port)
                   .IsOk());
  ASSERT_FALSE(
      device1
          ->SetUp("a.package", "", "", "", "", "", false, false, &devtools_port)
          .IsOk());
  ASSERT_TRUE(device1
                  ->SetUp("a.package", "an.activity", "", "", "", "", false,
                          false, &devtools_port)
                  .IsOk());
  ASSERT_TRUE(device1->TearDown().IsOk());
  ASSERT_TRUE(device1
                  ->SetUp("a.package", "an.activity", "a.process", "", "", "",
                          false, false, &devtools_port)
                  .IsOk());
  ASSERT_TRUE(device1->TearDown().IsOk());
  ASSERT_TRUE(device1
                  ->SetUp("a.package", "an.activity", "a.process",
                          "a.deviceSocket", "", "", false, false,
                          &devtools_port)
                  .IsOk());
  ASSERT_TRUE(device1->TearDown().IsOk());
  ASSERT_TRUE(device1
                  ->SetUp("a.package", "an.activity", "a.process",
                          "a.deviceSocket", "an.execName", "", false, false,
                          &devtools_port)
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
                          &devtools_port)
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
                          &devtools_port)
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
                 &devtools_port);
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
                 &devtools_port);
  device1->TearDown();
  ASSERT_FALSE(adb.KillForwardPortIsCalled());
}
