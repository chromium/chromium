// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/device_manager.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/chromedriver/chrome/adb.h"
#include "chrome/test/chromedriver/chrome/status.h"

const char kChromeCmdLineFile[] = "/data/local/tmp/chrome-command-line";

Device::Device(
    const std::string& device_serial, Adb* adb,
    base::Callback<void()> release_callback)
    : serial_(device_serial),
      adb_(adb),
      release_callback_(release_callback) {}

Device::~Device() {
  release_callback_.Run();
}

// Only allow completely alpha exec names.
bool IsValidExecName(const std::string& exec_name) {
  return std::find_if_not(exec_name.begin(), exec_name.end(), [](char ch) {
           return base::IsAsciiAlpha(ch);
         }) == exec_name.end();
}

Status Device::SetUp(const std::string& package,
                     const std::string& activity,
                     const std::string& process,
                     const std::string& device_socket,
                     const std::string& exec_name,
                     const std::string& args,
                     bool use_running_app,
                     int* devtools_port) {
  if (!active_package_.empty())
    return Status(kUnknownError,
        active_package_ + " was launched and has not been quit");

  Status status = adb_->CheckAppInstalled(serial_, package);
  if (status.IsError())
    return status;

  std::string known_activity;
  std::string command_line_file;
  std::string known_device_socket;
  std::string known_exec_name;
  bool use_debug_flag = false;
  if (package.compare("org.chromium.content_shell_apk") == 0) {
    // Chromium content shell.
    known_activity = ".ContentShellActivity";
    known_device_socket = "content_shell_devtools_remote";
    command_line_file = "/data/local/tmp/content-shell-command-line";
    known_exec_name = "content_shell";
  } else if (package.find("chrome") != std::string::npos &&
             package.find("webview") == std::string::npos) {
    // Chrome.
    known_activity = "com.google.android.apps.chrome.Main";
    known_device_socket = "chrome_devtools_remote";
    command_line_file = kChromeCmdLineFile;
    known_exec_name = "chrome";
    use_debug_flag = true;
  } else if (!exec_name.empty() && IsValidExecName(exec_name)) {
    // Allow directly specifying executable file name -- uncommon scenario.
    known_exec_name = exec_name;
    known_device_socket = device_socket;
    command_line_file = base::StringPrintf("/data/local/tmp/%s_devtools_remote",
                                           exec_name.c_str());
    use_debug_flag = true;
  }

  if (!use_running_app) {
    if (use_debug_flag) {
      // Some apps (such as Google Chrome) read command line from different
      // locations depending on if the app debug flag is set. When the debug
      // flag is not set, they use a location not writable by ChromeDriver
      // (except on rooted devices). Setting the debug flag allows the apps to
      // read command line from a location writable by ChromeDriver.
      //
      // This is needed only when use_running_app is false, for two reasons:
      // * It's too late to set the command line if the app is already running.
      // * Setting the debug flag has the side effect of shutting down the app,
      //   preventing use_running_app from working.
      status = adb_->SetDebugApp(serial_, package);
      if (status.IsError())
        return status;
    }

    status = adb_->ClearAppData(serial_, package);
    if (status.IsError())
      return status;

    if (!known_activity.empty()) {
      if (!activity.empty() ||
          !process.empty())
        return Status(kUnknownError, "known package " + package +
                      " does not accept activity/process");
    } else if (activity.empty()) {
      return Status(kUnknownError, "WebView apps require activity name");
    }

    if (!command_line_file.empty()) {
      status = adb_->SetCommandLineFile(serial_, command_line_file,
                                        known_exec_name, args);
      if (status.IsError())
        return Status(
            kUnknownError,
            "Failed to set Chrome's command line file on device " + serial_,
            status);
    }

    status = adb_->Launch(serial_, package,
                          known_activity.empty() ? activity : known_activity);
    if (status.IsError())
      return status;

    active_package_ = package;
  }
  return this->ForwardDevtoolsPort(package, process, &known_device_socket,
                                   devtools_port);
}

Status Device::ForwardDevtoolsPort(const std::string& package,
                                   const std::string& process,
                                   std::string* device_socket,
                                   int* devtools_port) {
  if (device_socket->empty()) {
    // Assume this is a WebView app.
    int pid;
    Status status = adb_->GetPidByName(serial_,
                                       process.empty() ? package : process,
                                       &pid);
    if (status.IsError()) {
      if (process.empty())
        status.AddDetails(
            "process name must be specified if not equal to package name");
      return status;
    }

    std::string socket_name;
    // The leading '@' means abstract UNIX sockets. Some apps have a custom
    // substring between the required "webview_devtools_remote_" prefix and
    // their PID, which Chrome DevTools accepts and we also should.
    std::string pattern =
        base::StringPrintf("@webview_devtools_remote_.*%d", pid);
    status = adb_->GetSocketByPattern(serial_, pattern, &socket_name);
    if (status.IsError()) {
      if (socket_name.empty())
        status.AddDetails(
            "make sure the app has its WebView configured for debugging");
      return status;
    }
    // When used in adb with "localabstract:", the leading '@' is not needed.
    *device_socket = socket_name.substr(1);
  }

  return adb_->ForwardPort(serial_, *device_socket, devtools_port);
}

Status Device::TearDown() {
  if (!active_package_.empty()) {
    std::string response;
    Status status = adb_->ForceStop(serial_, active_package_);
    if (status.IsError())
      return status;
    active_package_ = "";
  }
  return Status(kOk);
}

DeviceManager::DeviceManager(Adb* adb) : adb_(adb) {
  CHECK(adb_);
}

DeviceManager::~DeviceManager() {}

Status DeviceManager::AcquireDevice(std::unique_ptr<Device>* device) {
  std::vector<std::string> devices;
  Status status = adb_->GetDevices(&devices);
  if (status.IsError())
    return status;

  if (devices.empty())
    return Status(kUnknownError, "There are no devices online");

  base::AutoLock lock(devices_lock_);
  status = Status(kUnknownError, "All devices are in use (" +
                                     base::NumberToString(devices.size()) +
                                     " online)");
  std::vector<std::string>::iterator iter;
  for (iter = devices.begin(); iter != devices.end(); iter++) {
    if (!IsDeviceLocked(*iter)) {
      device->reset(LockDevice(*iter));
      status = Status(kOk);
      break;
    }
  }
  return status;
}

Status DeviceManager::AcquireSpecificDevice(const std::string& device_serial,
                                            std::unique_ptr<Device>* device) {
  std::vector<std::string> devices;
  Status status = adb_->GetDevices(&devices);
  if (status.IsError())
    return status;

  if (!base::Contains(devices, device_serial))
    return Status(kUnknownError,
        "Device " + device_serial + " is not online");

  base::AutoLock lock(devices_lock_);
  if (IsDeviceLocked(device_serial)) {
    status = Status(kUnknownError,
        "Device " + device_serial + " is already in use");
  } else {
    device->reset(LockDevice(device_serial));
    status = Status(kOk);
  }
  return status;
}

void DeviceManager::ReleaseDevice(const std::string& device_serial) {
  base::AutoLock lock(devices_lock_);
  active_devices_.remove(device_serial);
}

Device* DeviceManager::LockDevice(const std::string& device_serial) {
  active_devices_.push_back(device_serial);
  return new Device(device_serial, adb_,
      base::Bind(&DeviceManager::ReleaseDevice, base::Unretained(this),
                 device_serial));
}

bool DeviceManager::IsDeviceLocked(const std::string& device_serial) {
  return base::Contains(active_devices_, device_serial);
}
