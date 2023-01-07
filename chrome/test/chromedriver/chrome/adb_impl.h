// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_ADB_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_ADB_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/test/chromedriver/chrome/adb.h"

namespace base {
class SingleThreadTaskRunner;
}

class Status;

class AdbImpl : public Adb {
 public:
  explicit AdbImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      int port);
  ~AdbImpl() override;

  // Overridden from Adb:
  Status GetDevices(std::vector<std::string>* devices) override;
  Status ForwardPort(const std::string& device_serial,
                     const std::string& remote_abstract,
                     int* local_port_output) override;
  Status KillForwardPort(const std::string& device_serial,
                         int port) override;
  Status SetCommandLineFile(const std::string& device_serial,
                            const std::string& command_line_file,
                            const std::string& exec_name,
                            const std::string& args) override;
  Status CheckAppInstalled(const std::string& device_serial,
                           const std::string& package) override;
  Status ClearAppData(const std::string& device_serial,
                      const std::string& package) override;
  Status SetDebugApp(const std::string& device_serial,
                     const std::string& package) override;
  Status Launch(const std::string& device_serial,
                const std::string& package,
                const std::string& activity) override;
  Status ForceStop(const std::string& device_serial,
                   const std::string& package) override;
  Status GetPidByName(const std::string& device_serial,
                      const std::string& process_name,
                      int* pid) override;
  Status GetSocketByPattern(const std::string& device_serial,
                            const std::string& grep_pattern,
                            std::string* socket_name) override;

 private:
  Status ExecuteCommand(const std::string& command,
                        std::string* response);
  Status ExecuteHostCommand(const std::string& device_serial,
                            const std::string& host_command,
                            std::string* response);
  Status ExecuteHostShellCommand(const std::string& device_serial,
                                 const std::string& shell_command,
                                 std::string* response);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  int port_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_ADB_IMPL_H_
