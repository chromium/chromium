// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_MAC_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_MAC_H_

#include <memory>
#include <string>

#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_export.h"

class MacPreferences;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace policy {

class PolicyBundle;
class PolicyMap;
class Schema;

// A policy loader that loads policies from the Mac preferences system, and
// watches the managed preferences files for updates.
class POLICY_EXPORT PolicyLoaderMac : public AsyncPolicyLoader {
 public:
  PolicyLoaderMac(scoped_refptr<base::SequencedTaskRunner> task_runner,
                  const base::FilePath& managed_policy_path,
                  std::unique_ptr<MacPreferences> preferences);

  // |application_id| will be passed into Mac's Preference Utilities API
  // instead of the default value of kCFPreferencesCurrentApplication.
  PolicyLoaderMac(scoped_refptr<base::SequencedTaskRunner> task_runner,
                  const base::FilePath& managed_policy_path,
                  std::unique_ptr<MacPreferences> preferences,
                  CFStringRef application_id);
  PolicyLoaderMac(const PolicyLoaderMac&) = delete;
  PolicyLoaderMac& operator=(const PolicyLoaderMac&) = delete;

  ~PolicyLoaderMac() override;

  // AsyncPolicyLoader implementation.
  void InitOnBackgroundThread() override;
  PolicyBundle Load() override;
  base::Time LastModificationTime() override;

#if BUILDFLAG(IS_MAC)
  // Gets the path to the preferences (.plist) file associated with the given
  // |bundle_id|.  The file at the returned path might not exist (yet).
  // Returns an empty path upon failure.
  static base::FilePath GetManagedPolicyPath(CFStringRef bundle_id);
#endif

 private:
  // Callback for the FilePathWatcher.
  void OnFileUpdated(const base::FilePath& path, bool error);

  // Loads policies for the components described in the current schema_map()
  // which belong to the domain |domain_name|, and stores them in the |bundle|.
  void LoadPolicyForDomain(
      PolicyDomain domain,
      const std::string& domain_name,
      PolicyBundle* bundle);

  // Loads the policies described in |schema| from the bundle identified by
  // |bundle_id_string|, and stores them in |policy|.
  void LoadPolicyForComponent(const std::string& bundle_id_string,
                              const Schema& schema,
                              PolicyMap* policy);

  const std::unique_ptr<MacPreferences> preferences_;

  // Path to the managed preferences file for the current user, if it could
  // be found. Updates of this file trigger a policy reload.
  base::FilePath managed_policy_path_;

  // Watches for events on the |managed_policy_path_|.
  base::FilePathWatcher watcher_;

  // Application ID to pass into Mac's Preference Utilities API.
  base::apple::ScopedCFTypeRef<CFStringRef> application_id_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_MAC_H_
