// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_STATUS_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_STATUS_PROVIDER_H_

#include <memory>

#include "base/callback_helpers.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
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

// The following constants identify top-level keys in the dictionary returned by
// PolicyStatusProvider.
POLICY_EXPORT extern const char kAssetIdKey[];
POLICY_EXPORT extern const char kLocationKey[];
POLICY_EXPORT extern const char kDirectoryApiIdKey[];
POLICY_EXPORT extern const char kGaiaIdKey[];
POLICY_EXPORT extern const char kClientIdKey[];
POLICY_EXPORT extern const char kUsernameKey[];
POLICY_EXPORT extern const char kEnterpriseDomainManagerKey[];
POLICY_EXPORT extern const char kDomainKey[];

// An interface for querying the status of a policy provider.  It surfaces
// things like last fetch time or status of the backing store, but not the
// actual policies themselves.
class POLICY_EXPORT PolicyStatusProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPolicyStatusChanged() = 0;
  };

  PolicyStatusProvider();
  PolicyStatusProvider(const PolicyStatusProvider&) = delete;
  PolicyStatusProvider& operator=(const PolicyStatusProvider&) = delete;
  virtual ~PolicyStatusProvider();

  // Returns a dictionary with metadata about policies.
  virtual base::Value::Dict GetStatus();

  static base::Value::Dict GetStatusFromCore(const CloudPolicyCore* core);
  static base::Value::Dict GetStatusFromPolicyData(
      const enterprise_management::PolicyData* policy);

  // Overrides clock in tests. Returned closure removes the override when
  // destroyed.
  static base::ScopedClosureRunner OverrideClockForTesting(
      base::Clock* clock_for_testing);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyStatusChange();
  static std::u16string GetPolicyStatusFromStore(const CloudPolicyStore*,
                                                 const CloudPolicyClient*);
  static std::u16string GetTimeSinceLastActionString(base::Time);

 private:
  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_STATUS_PROVIDER_H_
