// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MANAGER_CLIENT_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace growth {

using CampaignComponentLoadedCallback = base::OnceCallback<void(
    const absl::optional<const base::FilePath>& file_path)>;

class CampaignsManagerClient {
 public:
  CampaignsManagerClient() = default;
  CampaignsManagerClient(const CampaignsManagerClient&) = delete;
  CampaignsManagerClient& operator=(const CampaignsManagerClient&) = delete;
  virtual ~CampaignsManagerClient() = default;

  // Loads campaigns component and trigger the `CampaignComponentLoadedCallback`
  // when loaded.
  virtual void LoadCampaignsComponent(
      CampaignComponentLoadedCallback callback) = 0;

  // True if the device is in demo mode.
  virtual bool IsDeviceInDemoMode() const = 0;

  // True if the device is cloud gaming device.
  virtual bool IsCloudGamingDevice() const = 0;

  // True if the device is feature aware device.
  virtual bool IsFeatureAwareDevice() const = 0;
};

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MANAGER_CLIENT_H_
