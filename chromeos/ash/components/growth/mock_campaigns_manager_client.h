// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_MOCK_CAMPAIGNS_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_MOCK_CAMPAIGNS_MANAGER_CLIENT_H_

#include "chromeos/ash/components/growth/campaigns_manager_client.h"
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
  MOCK_METHOD1(LoadCampaignsComponent,
               void(CampaignComponentLoadedCallback callback));
  MOCK_CONST_METHOD0(IsDeviceInDemoMode, bool());
  MOCK_CONST_METHOD0(IsCloudGamingDevice, bool());
  MOCK_CONST_METHOD0(IsFeatureAwareDevice, bool());
  MOCK_CONST_METHOD0(GetApplicationLocale, std::string&());
  MOCK_CONST_METHOD0(GetDemoModeAppVersion, const base::Version&());
};

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_MOCK_CAMPAIGNS_MANAGER_CLIENT_H_
