// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CONFIG_DIR_POLICY_LOADER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CONFIG_DIR_POLICY_LOADER_H_

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

namespace base {
class Value;
}

namespace policy {

// A policy loader implementation backed by a set of files in a given
// directory. The files should contain JSON-formatted policy settings. They are
// merged together and the result is returned in a PolicyBundle.
// The files are consulted in lexicographic file name order, so the
// last value read takes precedence in case of policy key collisions.
class POLICY_EXPORT ConfigDirPolicyLoader : public AsyncPolicyLoader {
 public:
  ConfigDirPolicyLoader(scoped_refptr<base::SequencedTaskRunner> task_runner,
                        const base::FilePath& config_dir,
                        PolicyScope scope);
  ~ConfigDirPolicyLoader() override;

  // AsyncPolicyLoader implementation.
  void InitOnBackgroundThread() override;
  std::unique_ptr<PolicyBundle> Load() override;
  base::Time LastModificationTime() override;

 private:
  // Loads the policy files at |path| into the |bundle|, with the given |level|.
  void LoadFromPath(const base::FilePath& path,
                    PolicyLevel level,
                    PolicyBundle* bundle);

  // Merges the 3rd party |policies| (extension policies) into the |bundle|,
  // with the given |level|.
  void Merge3rdPartyPolicy(const base::Value* policies,
                           PolicyLevel level,
                           PolicyBundle* bundle,
                           bool signin_profile = false);

  // Callback for the FilePathWatchers.
  void OnFileUpdated(const base::FilePath& path, bool error);

  // Task runner for running background jobs.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The directory containing the policy files.
  const base::FilePath config_dir_;

  // Policies loaded by this provider will have this scope.
  const PolicyScope scope_;

  // Watchers for events on the mandatory and recommended subdirectories of
  // |config_dir_|.
  base::FilePathWatcher mandatory_watcher_;
  base::FilePathWatcher recommended_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ConfigDirPolicyLoader);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CONFIG_DIR_POLICY_LOADER_H_
