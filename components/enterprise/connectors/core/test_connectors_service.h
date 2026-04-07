// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_TEST_CONNECTORS_SERVICE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_TEST_CONNECTORS_SERVICE_H_

#include "components/enterprise/connectors/core/connectors_service_base.h"
#include "components/prefs/testing_pref_service.h"

namespace enterprise_connectors {

// Fake Connectors Service that is only used for unit testing in components.
class TestConnectorsService : public ConnectorsServiceBase {
 public:
  explicit TestConnectorsService(TestingPrefServiceSimple* prefs);
  ~TestConnectorsService() override;

  // Set the machine dm token if needed.
  void set_machine_dm_token(const char* dm_token);

  // Set the profile dm token if needed.
  void set_profile_dm_token(const char* dm_token);

  // Set the specific analysis connectors to be enabled.
  void set_connectors_enabled(bool enabled);

  // Get the machine or profile dm token based on the value passed in by
  // TestingPrefServiceSimple.
  std::optional<ConnectorsServiceBase::DmToken> GetDmToken(
      const char* scope_pref) const override;

  // Returns std::nullopt.
  std::optional<std::string> GetBrowserDmToken() const override;

  // Returns a nullptr.
  std::unique_ptr<ClientMetadata> BuildClientMetadata(bool is_cloud) override;

  // returns true if the connectors is enabled.
  bool ConnectorsEnabled() const override;

  // Returns the pref service.
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;

  // NOTREACHED.
  policy::CloudPolicyManager* GetManagedUserCloudPolicyManager() const override;

 private:
  bool connectors_enabled_ = false;
  std::optional<ConnectorsServiceBase::DmToken> machine_dm_token_;
  std::optional<ConnectorsServiceBase::DmToken> profile_dm_token_;
  raw_ptr<TestingPrefServiceSimple> prefs_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_TEST_CONNECTORS_SERVICE_H_
