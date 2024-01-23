// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/win/group_policy_manager.h"

#include <userenv.h>

#include <optional>
#include <ostream>
#include <string>

#include "base/check.h"
#include "base/enterprise_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_generic.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

namespace {

struct ScopedHCriticalPolicySectionTraits {
  static HANDLE InvalidValue() { return nullptr; }
  static void Free(HANDLE handle) {
    if (handle != InvalidValue()) {
      ::LeaveCriticalPolicySection(handle);
    }
  }
};

// Manages the lifetime of critical policy section handle allocated by
// ::EnterCriticalPolicySection.
using scoped_hpolicy =
    base::ScopedGeneric<HANDLE, updater::ScopedHCriticalPolicySectionTraits>;

struct PolicySectionEvents
    : public base::RefCountedThreadSafe<PolicySectionEvents> {
  base::WaitableEvent enter_policy_section;
  base::WaitableEvent leave_policy_section;

 private:
  friend class base::RefCountedThreadSafe<PolicySectionEvents>;
  virtual ~PolicySectionEvents() = default;
};

base::Value::Dict LoadGroupPolicies(bool should_take_policy_critical_section) {
  base::ScopedClosureRunner leave_policy_section_closure;

  if (should_take_policy_critical_section && base::IsManagedDevice()) {
    // Only for managed machines, a best effort is made to take the Group Policy
    // critical section. Lock acquisition can take a long time in the worst case
    // scenarios, hence a short timed wait is used.

    auto events = base::MakeRefCounted<PolicySectionEvents>();
    leave_policy_section_closure.ReplaceClosure(base::BindOnce(
        [](scoped_refptr<PolicySectionEvents> events) {
          events->leave_policy_section.Signal();
        },
        events));

    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
        base::BindOnce(
            [](scoped_refptr<PolicySectionEvents> events) {
              scoped_hpolicy policy_lock(::EnterCriticalPolicySection(true));

              events->enter_policy_section.Signal();

              events->leave_policy_section.Wait();
            },
            events));

    if (!events->enter_policy_section.TimedWait(base::Seconds(30))) {
      VLOG(1) << "Timed out trying to get the policy critical section.";
    }
  }

  base::Value::Dict policies;

  for (base::win::RegistryValueIterator it(HKEY_LOCAL_MACHINE,
                                           UPDATER_POLICIES_KEY);
       it.Valid(); ++it) {
    const std::string key_name =
        base::ToLowerASCII(base::SysWideToUTF8(it.Name()));
    switch (it.Type()) {
      case REG_SZ:
        policies.Set(key_name, base::SysWideToUTF8(it.Value()));
        break;

      case REG_DWORD:
        policies.Set(key_name, *(reinterpret_cast<const int*>(it.Value())));
        break;

      default:
        // Ignore all types that are not used by updater policies.
        break;
    }
  }

  return policies;
}

}  // namespace

GroupPolicyManager::GroupPolicyManager(
    bool should_take_policy_critical_section,
    const std::optional<bool>& override_is_managed_device)
    : PolicyManager(LoadGroupPolicies(should_take_policy_critical_section)),
      is_managed_device_(override_is_managed_device.value_or(
          base::IsManagedOrEnterpriseDevice())) {}

GroupPolicyManager::~GroupPolicyManager() = default;

bool GroupPolicyManager::HasActiveDevicePolicies() const {
  return is_managed_device_ && PolicyManager::HasActiveDevicePolicies();
}

std::string GroupPolicyManager::source() const {
  return kSourceGroupPolicyManager;
}

}  // namespace updater
