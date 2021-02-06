// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_SYNCABLE_FACTORY_H_
#define COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_SYNCABLE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/pref_value_store.h"

namespace policy {
class BrowserPolicyConnector;
class PolicyService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace sync_preferences {

class PrefModelAssociatorClient;
class PrefServiceSyncable;

// A PrefServiceFactory that also knows how to build a
// PrefServiceSyncable, and may know about Chrome concepts such as
// PolicyService.
class PrefServiceSyncableFactory : public PrefServiceFactory {
 public:
  PrefServiceSyncableFactory();
  ~PrefServiceSyncableFactory() override;

  // Set up policy pref stores using the given policy service and connector.
  // These will assert when policy is not used.
  void SetManagedPolicies(policy::PolicyService* service,
                          policy::BrowserPolicyConnector* connector);
  void SetRecommendedPolicies(policy::PolicyService* service,
                              policy::BrowserPolicyConnector* connector);

  void SetPrefModelAssociatorClient(
      PrefModelAssociatorClient* pref_model_associator_client);

  std::unique_ptr<PrefServiceSyncable> CreateSyncable(
      scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry);

 private:
  PrefModelAssociatorClient* pref_model_associator_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceSyncableFactory);
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_SYNCABLE_FACTORY_H_
