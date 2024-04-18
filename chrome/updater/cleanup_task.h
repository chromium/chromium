// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CLEANUP_TASK_H_
#define CHROME_UPDATER_CLEANUP_TASK_H_

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

class Configurator;

// The Cleanup houses both periodic and one-time cleanup work items. For
// example, it is used to clean up obsolete files that were in-use at the time
// setup ran but can be cleaned up now.
class CleanupTask : public base::RefCountedThreadSafe<CleanupTask> {
 public:
  CleanupTask(UpdaterScope scope, scoped_refptr<Configurator> config);
  void Run(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<CleanupTask>;
  virtual ~CleanupTask();

  SEQUENCE_CHECKER(sequence_checker_);
  UpdaterScope scope_;
  scoped_refptr<Configurator> config_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_CLEANUP_TASK_H_
