// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MANAGER_CLIENT_H_

#include <map>
#include <memory>
#include <optional>
#include <variant>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/growth/action_performer.h"
#include "chromeos/ash/components/growth/campaigns_constants.h"

namespace base {
class Version;
}  // namespace base

namespace signin {
class IdentityManager;
}  // namespace signin

namespace growth {

using CampaignComponentLoadedCallback = base::OnceCallback<void(
    const std::optional<const base::FilePath>& file_path)>;

using OnTrackerInitializedCallback =
    base::OnceCallback<void(bool init_success)>;

using ActionMap = std::map<ActionType, std::unique_ptr<ActionPerformer>>;

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

  virtual void AddOnTrackerInitializedCallback(
      OnTrackerInitializedCallback callback) = 0;

  // True if the device is in demo mode.
  virtual bool IsDeviceInDemoMode() const = 0;

  // True if the device is cloud gaming device.
  virtual bool IsCloudGamingDevice() const = 0;

  // True if the device is feature aware device.
  virtual bool IsFeatureAwareDevice() const = 0;

  // True if the app icon is available on shelf.
  virtual bool IsAppIconOnShelf(const std::string& app_id) const = 0;

  // Returns application locale.
  virtual const std::string& GetApplicationLocale() const = 0;

  // Returns user selected locale.
  virtual const std::string& GetUserLocale() const = 0;

  // Returns the permanent country code stored for this client.
  // Country code is in the format of lowercase ISO 3166-1 alpha-2.
  // Example: `us`, `br`, `in`.
  virtual const std::string GetCountryCode() const = 0;

  // Get demo mode app component version.
  virtual const base::Version& GetDemoModeAppVersion() const = 0;

  // Get the implementations for the various Actions on the growth
  // framework.
  // TODO: b/330930157 - Rename to BuildCampaignsActions.
  virtual ActionMap GetCampaignsActions() = 0;

  // Register sythetical trial for current session.
  virtual void RegisterSyntheticFieldTrial(
      const std::string& trial_name,
      const std::string& group_name) const = 0;

  // Proxy to Feature Engagement methods.
  virtual void ClearConfig(
      const std::map<std::string, std::string>& params) = 0;
  virtual void RecordEvent(const std::string& event,
                           bool trigger_campaigns) = 0;
  virtual bool WouldTriggerHelpUI(
      const std::map<std::string, std::string>& params) = 0;
  // Returns the IdentityManager for the active user profile.
  virtual signin::IdentityManager* GetIdentityManager() const = 0;
};

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MANAGER_CLIENT_H_
