// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_MOCK_CAMPAIGNS_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_MOCK_CAMPAIGNS_MANAGER_CLIENT_H_

#include "chromeos/ash/components/growth/campaigns_manager_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace growth {

// A mock implementation of CampaignsManagerClient for use in tests.
class MockCampaignsManagerClient : public CampaignsManagerClient {
 public:
  MockCampaignsManagerClient();
  MockCampaignsManagerClient(const MockCampaignsManagerClient&) = delete;
  MockCampaignsManagerClient& operator=(const MockCampaignsManagerClient&) =
      delete;
  ~MockCampaignsManagerClient() override;

  // CampaignsManagerClient:
  MOCK_METHOD(void,
              LoadCampaignsComponent,
              (CampaignComponentLoadedCallback callback),
              (override));
  MOCK_METHOD(void,
              AddOnTrackerInitializedCallback,
              (OnTrackerInitializedCallback callback),
              (override));
  MOCK_METHOD(bool, IsDeviceInDemoMode, (), (const, override));
  MOCK_METHOD(bool, IsCloudGamingDevice, (), (const, override));
  MOCK_METHOD(bool, IsFeatureAwareDevice, (), (const, override));
  MOCK_METHOD(bool,
              IsAppIconOnShelf,
              (const std::string& app_id),
              (const, override));
  MOCK_METHOD(std::string&, GetApplicationLocale, (), (const, override));
  MOCK_METHOD(std::string&, GetUserLocale, (), (const, override));
  MOCK_METHOD(const std::string, GetCountryCode, (), (const, override));
  MOCK_METHOD(const base::Version&,
              GetDemoModeAppVersion,
              (),
              (const, override));
  MOCK_METHOD(ActionMap, GetCampaignsActions, (), (override));
  MOCK_METHOD(void,
              RegisterSyntheticFieldTrial,
              (const std::string& trial_name, const std::string& group_name),
              (const, override));
  MOCK_METHOD(void,
              ClearConfig,
              ((const std::map<std::string, std::string>& params)),
              (override));
  MOCK_METHOD(void,
              RecordEvent,
              (const std::string& event, bool trigger_campaigns),
              (override));
  MOCK_METHOD(bool,
              WouldTriggerHelpUI,
              ((const std::map<std::string, std::string>& params)),
              (override));
  MOCK_METHOD(signin::IdentityManager*,
              GetIdentityManager,
              (),
              (const, override));
};

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_MOCK_CAMPAIGNS_MANAGER_CLIENT_H_
