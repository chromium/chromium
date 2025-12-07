// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_HANDLE_INCONSISTENT_APPS_TASK_H_
#define CHROME_UPDATER_HANDLE_INCONSISTENT_APPS_TASK_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/updater_scope.h"
#include "components/update_client/update_client.h"

namespace base {
class SequencedTaskRunner;
}

namespace updater {
class Configurator;

// Detect and reconcile apps whose state is inconsistent with the updater's
// persisted data. This consists of: registering apps from legacy updaters, and
// sending install pings for apps which have been updated external to the
// updater.
class HandleInconsistentAppsTask
    : public base::RefCountedThreadSafe<HandleInconsistentAppsTask> {
 public:
  HandleInconsistentAppsTask(scoped_refptr<Configurator> config,
                             UpdaterScope scope);
  void Run(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<HandleInconsistentAppsTask>;
  virtual ~HandleInconsistentAppsTask();

  void FindUnregisteredApps(base::OnceClosure callback);
  void PingOverinstalledApps(base::OnceClosure callback);
  void SendOverinstallPings(base::OnceClosure callback,
                            std::vector<update_client::CrxComponent> pings);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<Configurator> config_;
  UpdaterScope scope_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_HANDLE_INCONSISTENT_APPS_TASK_H_
