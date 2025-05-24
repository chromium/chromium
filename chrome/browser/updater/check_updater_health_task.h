// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_CHECK_UPDATER_HEALTH_TASK_H_
#define CHROME_BROWSER_UPDATER_CHECK_UPDATER_HEALTH_TASK_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

class CheckUpdaterHealthTask
    : public base::RefCountedThreadSafe<CheckUpdaterHealthTask> {
 public:
  explicit CheckUpdaterHealthTask(UpdaterScope scope);
  void Run(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<CheckUpdaterHealthTask>;
  virtual ~CheckUpdaterHealthTask();

  void CheckAndRecordUpdaterHealth(const base::Version& version);

  SEQUENCE_CHECKER(sequence_checker_);
  const UpdaterScope scope_;
};

}  // namespace updater

#endif  // CHROME_BROWSER_UPDATER_CHECK_UPDATER_HEALTH_TASK_H_
