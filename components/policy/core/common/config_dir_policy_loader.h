// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CONFIG_DIR_POLICY_LOADER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CONFIG_DIR_POLICY_LOADER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include <sys/types.h>
#endif

namespace base {
class Value;
}

namespace policy {

// A policy loader implementation backed by a set of files in one or more
// directories. The files should contain JSON-formatted policy settings. They
// are merged together and the result is returned in a PolicyBundle.
// Within a directory, the files are consulted in lexicographic file name order,
// so the last value read takes precedence in case of policy key collisions.
// Across directories, earlier directories in `config_dirs` take precedence over
// later ones at the same PolicyLevel.
class POLICY_EXPORT ConfigDirPolicyLoader : public AsyncPolicyLoader {
 public:
  // Requires that a policy directory, every parent directory up to
  // `trusted_base_dir`, its `managed/` and `recommended/` subdirectories, and
  // each policy file read from it be owned by `trusted_uid`, not be symlinks,
  // and not be writable by group or others. Symlinks in
  // `ConfigDirectory::path` itself are resolved before checking ownership.
  // Ownership can only be verified on Linux and Mac; `OwnershipRequirement`
  // must not be used on other platforms.
  struct OwnershipRequirement {
    base::FilePath trusted_base_dir;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    uid_t trusted_uid = 0;
#endif
  };

  // Describes a policy directory and any ownership check required to trust it.
  struct POLICY_EXPORT ConfigDirectory {
    explicit ConfigDirectory(base::FilePath path);
    ConfigDirectory(base::FilePath path,
                    std::optional<OwnershipRequirement> ownership_requirement);
    ConfigDirectory(const ConfigDirectory&);
    ConfigDirectory& operator=(const ConfigDirectory&);
    ConfigDirectory(ConfigDirectory&&);
    ConfigDirectory& operator=(ConfigDirectory&&);
    ~ConfigDirectory();

    base::FilePath path;
    std::optional<OwnershipRequirement> ownership_requirement;
  };

  ConfigDirPolicyLoader(scoped_refptr<base::SequencedTaskRunner> task_runner,
                        std::vector<ConfigDirectory> config_dirs,
                        PolicyScope scope);
  ConfigDirPolicyLoader(const ConfigDirPolicyLoader&) = delete;
  ConfigDirPolicyLoader& operator=(const ConfigDirPolicyLoader&) = delete;
  ~ConfigDirPolicyLoader() override;

  // AsyncPolicyLoader implementation.
  void InitOnBackgroundThread() override;
  PolicyBundle Load() override;
  base::Time LastModificationTime() override;

 private:
  // Loads the policy files at |path| into the |bundle|, with the given |level|.
  // Files that fail `config_dir`'s ownership check (if any) are skipped.
  void LoadFromPath(const base::FilePath& path,
                    PolicyLevel level,
                    const ConfigDirectory& config_dir,
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

  // Policy directories in decreasing order of precedence.
  const std::vector<ConfigDirectory> config_dirs_;

  // Policies loaded by this provider will have this scope.
  const PolicyScope scope_;

  // Watchers for events on the mandatory and recommended subdirectories of
  // `config_dirs_`.
  std::vector<std::unique_ptr<base::FilePathWatcher>> watchers_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CONFIG_DIR_POLICY_LOADER_H_
