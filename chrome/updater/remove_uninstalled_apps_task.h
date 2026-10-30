// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_REMOVE_UNINSTALLED_APPS_TASK_H_
#define CHROME_UPDATER_REMOVE_UNINSTALLED_APPS_TASK_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"

namespace base {
class FilePath;
}  // namespace base

namespace update_client {
class UpdateClient;
enum class Error;
}  // namespace update_client

namespace updater {
class Configurator;

class RemoveUninstalledAppsTask
    : public base::RefCountedThreadSafe<RemoveUninstalledAppsTask> {
 public:
  // `is_installing` returns true for apps with an install in progress. Such
  // apps are not removed.
  RemoveUninstalledAppsTask(
      scoped_refptr<Configurator> config,
      UpdaterScope scope,
      base::RepeatingCallback<bool(const std::string&)> is_installing);
  void Run(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<RemoveUninstalledAppsTask>;
  virtual ~RemoveUninstalledAppsTask();

  std::optional<UninstallPingReason> GetUnregisterReason(
      const std::string& app_id,
      const base::FilePath& ecp) const;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<Configurator> config_;
  scoped_refptr<update_client::UpdateClient> update_client_;
  UpdaterScope scope_;
  base::RepeatingCallback<bool(const std::string&)> is_installing_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_REMOVE_UNINSTALLED_APPS_TASK_H_
