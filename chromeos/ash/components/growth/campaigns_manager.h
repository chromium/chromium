// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MANAGER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/growth/campaigns_manager_client.h"
#include "chromeos/ash/components/growth/campaigns_matcher.h"
#include "chromeos/ash/components/growth/campaigns_model.h"

class PrefService;

namespace growth {

// A class that manages growth campaigns.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH) CampaignsManager {
 public:
  using GetCampaignCallback =
      base::OnceCallback<void(const Campaign* campaign)>;

  // Interface for observing the CampaignsManager.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Trigger when complete loading growth campaigns. CampaignsManager is ready
    // for serving campaigns at this point.
    virtual void OnCampaignsLoadCompleted() = 0;
  };

  CampaignsManager(CampaignsManagerClient* client, PrefService* local_state);
  CampaignsManager(const CampaignsManager&) = delete;
  CampaignsManager& operator=(const CampaignsManager&) = delete;
  ~CampaignsManager();

  // Static.
  static CampaignsManager* Get();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetPrefs(PrefService* prefs);

  // Download and install campaigns. Once installed, trigger the
  // `OnCampaignsLoaded` to install campaigns and notifier observers when
  // complete loading campaigns.
  void LoadCampaigns();

  // Get campaigns by slot. This is used by reactive slots to query campaign
  // that targets the given `slot`.
  const Campaign* GetCampaignBySlot(Slot slot) const;

 private:
  // Triggred when campaigns component loaded.
  void OnCampaignsComponentLoaded(
      const absl::optional<const base::FilePath>& file_path);

  // Triggered when campaigns are loaded from the campaigns component mounted
  // path.
  void OnCampaignsLoaded(absl::optional<base::Value::Dict> campaigns);

  // Notify observers that campaigns are loaded and CampaignsManager is ready
  // to query.
  void NotifyCampaignsLoaded();

  raw_ptr<CampaignsManagerClient, ExperimentalAsh> client_ = nullptr;

  // True if campaigns are loaded.
  bool campaigns_loaded_ = false;

  // Campaigns store owns all campaigns, including proactive and reactive
  // campaigns.
  CampaignsStore campaigns_store_;
  // Campaigns matcher for selecting campaigns based on criterias.
  CampaignsMatcher matcher_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CampaignsManager> weak_factory_{this};
};

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_MANAGER_H_
