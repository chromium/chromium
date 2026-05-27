// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_POLICY_STORE_OBSERVER_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_POLICY_STORE_OBSERVER_H_

#include <string>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

// Observes a `policy::CloudPolicyStore` and runs a callback with the
// appropriate enterprise disclaimer text once the store is loaded,
// or if a timeout/error occurs. Used in the Chrome First Run Experience (FRE)
// to display device management disclaimers.
class PolicyStoreObserver : public policy::CloudPolicyStore::Observer {
 public:
  explicit PolicyStoreObserver(
      base::OnceCallback<void(std::string)> handle_policy_store_change);

  PolicyStoreObserver(const PolicyStoreObserver&) = delete;
  PolicyStoreObserver& operator=(const PolicyStoreObserver&) = delete;

  ~PolicyStoreObserver() override;

  // policy::CloudPolicyStore::Observer:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

 private:
  void OnOrganizationFetchTimeout();

  base::ScopedObservation<policy::CloudPolicyStore,
                          policy::CloudPolicyStore::Observer>
      policy_store_observation_{this};
  base::OnceCallback<void(std::string)> handle_policy_store_change_;
  base::CancelableOnceCallback<void()> on_organization_fetch_timeout_;
  base::TimeTicks start_time_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_POLICY_STORE_OBSERVER_H_
