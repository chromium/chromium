// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MATCHER_H_

#include "base/time/time.h"
#include "chromeos/ash/components/growth/campaigns_manager_client.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "url/gurl.h"

class PrefService;

namespace growth {

class CampaignsMatcher {
 public:
  CampaignsMatcher(CampaignsManagerClient* client, PrefService* local_state);
  CampaignsMatcher(const CampaignsMatcher&) = delete;
  CampaignsMatcher& operator=(const CampaignsMatcher&) = delete;
  ~CampaignsMatcher();

  // Prefs related targeting will only be enabled after this method is call
  // explicitly to set user prefs.
  void SetPrefs(PrefService* prefs);

  void SetCampaigns(const CampaignsPerSlot* campaigns);

  const std::string& opened_app_id() const { return opened_app_id_; }
  void SetOpenedApp(const std::string& app_id);
  void SetOobeCompleteTime(base::Time time);

  const GURL& active_url() const { return active_url_; }
  void SetActiveUrl(const GURL& url);

  // Select the targeted campaign for the given `slot`. Returns nullptr if no
  // campaign found for the given `slot`.
  const Campaign* GetCampaignBySlot(Slot slot) const;

 private:
  bool MatchDemoModeTier(const DemoModeTargeting& targeting) const;
  bool MatchDemoModeAppVersion(const DemoModeTargeting& targeting) const;
  bool MatchRetailers(const base::Value::List* retailers) const;
  bool MaybeMatchDemoModeTargeting(const DemoModeTargeting& targeting) const;
  bool MatchMilestone(const DeviceTargeting& targeting) const;
  bool MatchDeviceTargeting(const DeviceTargeting& targeting) const;
  bool MatchRegisteredTime(const std::unique_ptr<TimeWindowTargeting>&
                               registered_time_targeting) const;
  bool MatchExperimentTagTargeting(const base::Value::List* targeting) const;
  bool MatchOpenedApp(const std::vector<std::unique_ptr<AppTargeting>>&
                          apps_opened_targeting) const;
  bool MatchActiveUrlRegexes(
      const std::vector<std::string>& active_url_regrexes) const;
  bool MatchSessionTargeting(const SessionTargeting& targeting) const;
  bool MatchRuntimeTargeting(const RuntimeTargeting& targeting,
                             int campaign_id) const;
  bool MatchEvents(std::unique_ptr<EventsTargeting> config,
                   int campaign_id) const;
  bool MatchMinorUser(std::optional<bool> minor_user_targeting) const;
  bool Matched(const Targetings* targetings, int campaign_id) const;

  // Owned by CampaignsManager.
  raw_ptr<const CampaignsPerSlot> campaigns_ = nullptr;
  raw_ptr<CampaignsManagerClient> client_ = nullptr;
  raw_ptr<PrefService> local_state_ = nullptr;
  raw_ptr<PrefService> prefs_ = nullptr;
  std::string opened_app_id_;
  GURL active_url_;
  base::Time oobe_compelete_time_;
};

}  // namespace growth
#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MATCHER_H_
