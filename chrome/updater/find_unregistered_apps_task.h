// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_FIND_UNREGISTERED_APPS_TASK_H_
#define CHROME_UPDATER_FIND_UNREGISTERED_APPS_TASK_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/updater_scope.h"

namespace updater {
class Configurator;

class FindUnregisteredAppsTask
    : public base::RefCountedThreadSafe<FindUnregisteredAppsTask> {
 public:
  FindUnregisteredAppsTask(scoped_refptr<Configurator> config,
                           UpdaterScope scope);
  void Run(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<FindUnregisteredAppsTask>;
  virtual ~FindUnregisteredAppsTask();

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<Configurator> config_;
  UpdaterScope scope_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_FIND_UNREGISTERED_APPS_TASK_H_
