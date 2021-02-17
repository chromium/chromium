// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVICE_MANAGER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVICE_MANAGER_H_

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

class Adb;
class Status;
class DeviceManager;

class Device {
 public:
  ~Device();

  Status SetUp(const std::string& package,
               const std::string& activity,
               const std::string& process,
               const std::string& device_socket,
               const std::string& exec_name,
               const std::string& args,
               bool use_running_app,
               int* port);

  Status TearDown();

 private:
  friend class DeviceManager;

  Device(const std::string& device_serial,
         Adb* adb,
         base::OnceCallback<void()> release_callback);

  Status ForwardDevtoolsPort(const std::string& package,
                             const std::string& process,
                             std::string* device_socket,
                             int* devtools_port);

  const std::string serial_;
  std::string active_package_;
  Adb* adb_;
  int devtools_port_ = 0;
  base::OnceCallback<void()> release_callback_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

class DeviceManager {
 public:
  explicit DeviceManager(Adb* adb);
  ~DeviceManager();

  // Returns a device which will not be reassigned during its lifetime.
  Status AcquireDevice(std::unique_ptr<Device>* device);

  // Returns a device with the same guarantees as AcquireDevice, but fails
  // if the device with the given serial number is not avaliable.
  Status AcquireSpecificDevice(const std::string& device_serial,
                               std::unique_ptr<Device>* device);

 private:
  void ReleaseDevice(const std::string& device_serial);

  Device* LockDevice(const std::string& device_serial);
  bool IsDeviceLocked(const std::string& device_serial);

  base::Lock devices_lock_;
  std::list<std::string> active_devices_;
  Adb* adb_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManager);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVICE_MANAGER_H_
