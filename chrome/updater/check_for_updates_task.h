// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CHECK_FOR_UPDATES_TASK_H_
#define CHROME_UPDATER_CHECK_FOR_UPDATES_TASK_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

namespace update_client {
class UpdateClient;
enum class Error;
}  // namespace update_client

namespace updater {
class Configurator;
class PersistedData;

class CheckForUpdatesTask
    : public base::RefCountedThreadSafe<CheckForUpdatesTask> {
 public:
  CheckForUpdatesTask(
      scoped_refptr<Configurator> config,
      UpdaterScope scope,
      const std::string& task_name,
      base::OnceCallback<void(base::OnceCallback<void(UpdateService::Result)>)>
          update_checker);
  void Run(base::OnceClosure callback);

 private:
  using UpdateChecker =
      base::OnceCallback<void(base::OnceCallback<void(UpdateService::Result)>)>;

  friend class base::RefCountedThreadSafe<CheckForUpdatesTask>;
  virtual ~CheckForUpdatesTask();

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<Configurator> config_;
  const std::string task_name_;
  UpdateChecker update_checker_;
  scoped_refptr<updater::PersistedData> persisted_data_;
  scoped_refptr<update_client::UpdateClient> update_client_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_CHECK_FOR_UPDATES_TASK_H_
