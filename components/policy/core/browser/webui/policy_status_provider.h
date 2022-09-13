// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_STATUS_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_STATUS_PROVIDER_H_

#include <memory>

#include "base/callback_helpers.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace base {
class Time;
}

namespace enterprise_management {
class PolicyData;
}

namespace policy {
class CloudPolicyClient;
class CloudPolicyCore;
class CloudPolicyStore;

POLICY_EXPORT extern const char kPolicyDescriptionKey[];

// An interface for querying the status of a policy provider.  It surfaces
// things like last fetch time or status of the backing store, but not the
// actual policies themselves.
class POLICY_EXPORT PolicyStatusProvider {
 public:
  PolicyStatusProvider();
  PolicyStatusProvider(const PolicyStatusProvider&) = delete;
  PolicyStatusProvider& operator=(const PolicyStatusProvider&) = delete;
  virtual ~PolicyStatusProvider();

  // Sets a callback to invoke upon status changes.
  virtual void SetStatusChangeCallback(const base::RepeatingClosure& callback);

  // Returns a dictionary with metadata about policies.
  virtual base::Value::Dict GetStatus();

  static base::Value::Dict GetStatusFromCore(const CloudPolicyCore* core);
  static base::Value::Dict GetStatusFromPolicyData(
      const enterprise_management::PolicyData* policy);

  // Overrides clock in tests. Returned closure removes the override when
  // destroyed.
  static base::ScopedClosureRunner OverrideClockForTesting(
      base::Clock* clock_for_testing);

 protected:
  void NotifyStatusChange();
  static std::u16string GetPolicyStatusFromStore(const CloudPolicyStore*,
                                                 const CloudPolicyClient*);
  static std::u16string GetTimeSinceLastActionString(base::Time);

 private:
  base::RepeatingClosure callback_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_STATUS_PROVIDER_H_
