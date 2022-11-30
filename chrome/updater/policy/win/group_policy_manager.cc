// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/win/group_policy_manager.h"

#include <ostream>
#include <string>

#include <userenv.h>

#include "base/check.h"
#include "base/enterprise_util.h"
#include "base/scoped_generic.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

namespace {

struct ScopedHCriticalPolicySectionTraits {
  static HANDLE InvalidValue() { return nullptr; }
  static void Free(HANDLE handle) {
    if (handle != InvalidValue())
      ::LeaveCriticalPolicySection(handle);
  }
};

// Manages the lifetime of critical policy section handle allocated by
// ::EnterCriticalPolicySection.
using scoped_hpolicy =
    base::ScopedGeneric<HANDLE, updater::ScopedHCriticalPolicySectionTraits>;

base::Value::Dict LoadGroupPolicies() {
  scoped_hpolicy policy_lock;

  if (base::IsManagedDevice()) {
    // GPO rules mandate a call to EnterCriticalPolicySection() before reading
    // policies (and a matching LeaveCriticalPolicySection() call after read).
    // Acquire the lock for managed machines because group policies are
    // applied only in this case, and the lock acquisition can take a long
    // time, in the worst case scenarios.
    policy_lock.reset(::EnterCriticalPolicySection(true));
    CHECK(policy_lock.is_valid()) << "Failed to get policy lock.";
  }

  base::Value::Dict policies;

  for (base::win::RegistryValueIterator it(HKEY_LOCAL_MACHINE,
                                           UPDATER_POLICIES_KEY);
       it.Valid(); ++it) {
    const std::string key_name = base::SysWideToUTF8(it.Name());
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

GroupPolicyManager::GroupPolicyManager() : PolicyManager(LoadGroupPolicies()) {}

GroupPolicyManager::~GroupPolicyManager() = default;

bool GroupPolicyManager::HasActiveDevicePolicies() const {
  return PolicyManager::HasActiveDevicePolicies() && base::IsManagedDevice();
}

std::string GroupPolicyManager::source() const {
  return std::string("GroupPolicy");
}

}  // namespace updater
