// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_REMOVE_UNINSTALLED_APPS_TASK_H_
#define CHROME_UPDATER_REMOVE_UNINSTALLED_APPS_TASK_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"

namespace update_client {
class UpdateClient;
enum class Error;
}  // namespace update_client

namespace updater {
class Configurator;
class PersistedData;

class RemoveUninstalledAppsTask
    : public base::RefCountedThreadSafe<RemoveUninstalledAppsTask> {
 public:
  explicit RemoveUninstalledAppsTask(scoped_refptr<Configurator> config);
  void Run(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<RemoveUninstalledAppsTask>;
  virtual ~RemoveUninstalledAppsTask();

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<Configurator> config_;
  scoped_refptr<PersistedData> persisted_data_;
  scoped_refptr<update_client::UpdateClient> update_client_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_REMOVE_UNINSTALLED_APPS_TASK_H_
