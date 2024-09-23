// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_WEBUI_MACHINE_LEVEL_USER_CLOUD_POLICY_STATUS_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_WEBUI_MACHINE_LEVEL_USER_CLOUD_POLICY_STATUS_PROVIDER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/policy_export.h"
#include "components/prefs/pref_service.h"

namespace policy {
class CloudPolicyCore;

// The following constants identify top-level keys in the dictionary returned by
// and are specific to MachineLevelUserCloudPolicyStatusProvider.
inline constexpr char kDeviceIdKey[] = "deviceId";
inline constexpr char kMachineKey[] = "machine";

struct POLICY_EXPORT MachineLevelUserCloudPolicyContext {
  std::string enrollmentToken;
  std::string deviceId;
  std::string lastReportTimestampPrefName;
};

class POLICY_EXPORT MachineLevelUserCloudPolicyStatusProvider
    : public PolicyStatusProvider,
      public CloudPolicyStore::Observer {
 public:
  MachineLevelUserCloudPolicyStatusProvider(
      CloudPolicyCore* core,
      PrefService* prefs,
      MachineLevelUserCloudPolicyContext* context);
  MachineLevelUserCloudPolicyStatusProvider(
      const MachineLevelUserCloudPolicyStatusProvider&) = delete;
  MachineLevelUserCloudPolicyStatusProvider& operator=(
      const MachineLevelUserCloudPolicyStatusProvider&) = delete;
  ~MachineLevelUserCloudPolicyStatusProvider() override;

  // PolicyStatusProvider implementation.
  base::Value::Dict GetStatus() override;

  // CloudPolicyStore::Observer implementation.
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

 private:
  raw_ptr<CloudPolicyCore> core_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<MachineLevelUserCloudPolicyContext> context_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_WEBUI_MACHINE_LEVEL_USER_CLOUD_POLICY_STATUS_PROVIDER_H_
