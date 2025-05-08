// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_ADB_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_ADB_H_

#include <string>
#include <vector>

#include "base/values.h"

class Status;

class Adb {
 public:
  virtual ~Adb() = default;

  virtual Status GetDevices(std::vector<std::string>* devices) = 0;
  virtual Status ForwardPort(const std::string& device_serial,
                             const std::string& remote_abstract,
                             int* local_port_output) = 0;
  virtual Status KillForwardPort(const std::string& device_serial,
                                 int port) = 0;
  virtual Status SetCommandLineFile(const std::string& device_serial,
                                    const std::string& command_line_file,
                                    const std::string& exec_name,
                                    const std::string& args) = 0;
  virtual Status CheckAppInstalled(const std::string& device_serial,
                                   const std::string& package) = 0;
  virtual Status ClearAppData(const std::string& device_serial,
                              const std::string& package) = 0;
  virtual Status SetDebugApp(const std::string& device_serial,
                             const std::string& package) = 0;
  virtual Status Launch(const std::string& device_serial,
                        const std::string& package,
                        const std::string& activity) = 0;
  virtual Status ForceStop(const std::string& device_serial,
                           const std::string& package) = 0;
  virtual Status GetPidByName(const std::string& device_serial,
                              const std::string& process_name,
                              int* pid) = 0;
  virtual Status GetSocketByPattern(const std::string& device_serial,
                                    const std::string& grep_pattern,
                                    std::string* socket_name) = 0;
  virtual Status SetPreferences(const std::string& device_serial,
                                const std::string& path,
                                const base::Value::Dict* custom_prefs) = 0;
  virtual Status SetLocalState(const std::string& device_serial,
                               const std::string& path,
                               const base::Value::Dict* custom_local_state) = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_ADB_H_
