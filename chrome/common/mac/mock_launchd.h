// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_MOCK_LAUNCHD_H_
#define CHROME_COMMON_MAC_MOCK_LAUNCHD_H_

#include <launch.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/common/multi_process_lock.h"

namespace base {
class SingleThreadTaskRunner;
}

// TODO(dmaclach): Write this in terms of a real mock.
// http://crbug.com/76923
class MockLaunchd : public Launchd {
 public:
  static bool MakeABundle(const base::FilePath& dst,
                          const std::string& name,
                          base::FilePath* bundle_root,
                          base::FilePath* executable);

  MockLaunchd(const base::FilePath& file,
              scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
              base::OnceClosure quit_closure,
              bool as_service);
  ~MockLaunchd() override;

  bool GetJobInfo(const std::string& label,
                  mac::services::JobInfo* info) override;
  bool RemoveJob(const std::string& label) override;
  bool RestartJob(Domain domain,
                  Type type,
                  CFStringRef name,
                  CFStringRef session_type) override;
  CFMutableDictionaryRef CreatePlistFromFile(Domain domain,
                                             Type type,
                                             CFStringRef name) override;
  bool WritePlistToFile(Domain domain,
                        Type type,
                        CFStringRef name,
                        CFDictionaryRef dict) override;
  bool DeletePlist(Domain domain, Type type, CFStringRef name) override;

  void SignalReady();

  bool restart_called() const { return restart_called_; }
  bool remove_called() const { return remove_called_; }
  bool checkin_called() const { return checkin_called_; }
  bool write_called() const { return write_called_; }
  bool delete_called() const { return delete_called_; }

 private:
  base::FilePath file_;
  std::string pipe_name_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  base::OnceClosure quit_closure_;
  std::unique_ptr<MultiProcessLock> running_lock_;
  bool as_service_;
  bool restart_called_;
  bool remove_called_;
  bool checkin_called_;
  bool write_called_;
  bool delete_called_;
};

#endif  // CHROME_COMMON_MAC_MOCK_LAUNCHD_H_
