// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_WIN_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_WIN_H_

#include <windows.h>

#include <userenv.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/object_watcher.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class PolicyMap;
class RegistryDict;

// Loads policies from the Windows registry, and watches for Group Policy
// notifications to trigger reloads.
class POLICY_EXPORT PolicyLoaderWin
    : public AsyncPolicyLoader,
      public base::win::ObjectWatcher::Delegate {
 public:
  PolicyLoaderWin(scoped_refptr<base::SequencedTaskRunner> task_runner,
                  const std::wstring& chrome_policy_key);
  ~PolicyLoaderWin() override;

  // Creates a policy loader that uses the Registry to access GPO.
  static std::unique_ptr<PolicyLoaderWin> Create(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const std::wstring& chrome_policy_key);

  // AsyncPolicyLoader implementation.
  void InitOnBackgroundThread() override;
  std::unique_ptr<PolicyBundle> Load() override;

 private:
  // Parses Chrome policy from |gpo_dict| for the given |scope| and |level| and
  // merges it into |chrome_policy_map|.
  void LoadChromePolicy(const RegistryDict* gpo_dict,
                        PolicyLevel level,
                        PolicyScope scope,
                        PolicyMap* chrome_policy_map);

  // Loads 3rd-party policy from |gpo_dict| and merges it into |bundle|.
  void Load3rdPartyPolicy(const RegistryDict* gpo_dict,
                          PolicyScope scope,
                          PolicyBundle* bundle);

  // Installs the watchers for the Group Policy update events.
  void SetupWatches();

  // ObjectWatcher::Delegate overrides:
  void OnObjectSignaled(HANDLE object) override;

  bool is_initialized_;
  const std::wstring chrome_policy_key_;

  base::WaitableEvent user_policy_changed_event_;
  base::WaitableEvent machine_policy_changed_event_;
  base::win::ObjectWatcher user_policy_watcher_;
  base::win::ObjectWatcher machine_policy_watcher_;
  bool user_policy_watcher_failed_;
  bool machine_policy_watcher_failed_;

  DISALLOW_COPY_AND_ASSIGN(PolicyLoaderWin);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_WIN_H_
