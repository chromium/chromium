// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_manager.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/growth/campaigns_matcher.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace growth {

namespace {

CampaignsManager* g_instance = nullptr;

inline constexpr char kCampaignFileName[] = "campaigns.json";

absl::optional<base::Value::Dict> ReadCampaignsFile(
    const base::FilePath& campaigns_component_path) {
  std::string campaigns_data;
  if (!base::ReadFileToString(
          campaigns_component_path.Append(kCampaignFileName),
          &campaigns_data)) {
    LOG(ERROR) << "Failed to read campaigns file from disk.";
    RecordCampaignsManagerError(CampaignsManagerError::kCampaignsFileLoadFail);
    return absl::nullopt;
  }

  absl::optional<base::Value> value(base::JSONReader::Read(campaigns_data));
  if (!value || !value->is_dict()) {
    LOG(ERROR) << "Failed to parse campaigns file.";
    RecordCampaignsManagerError(CampaignsManagerError::kCampaignsParsingFail);
    return absl::nullopt;
  }
  return std::move(value->GetDict());
}

}  // namespace

// static
CampaignsManager* CampaignsManager::Get() {
  DCHECK(g_instance);
  return g_instance;
}

CampaignsManager::CampaignsManager(CampaignsManagerClient* client,
                                   PrefService* local_state)
    : client_(client), matcher_(client, local_state) {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

CampaignsManager::~CampaignsManager() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void CampaignsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CampaignsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CampaignsManager::SetPrefs(PrefService* prefs) {
  // Pass `prefs` to `CampaignsMatcher` to enable prefs related targettings.
  matcher_.SetPrefs(prefs);
}

void CampaignsManager::LoadCampaigns(base::OnceClosure load_callback) {
  // TODO(b/299305911): Add metrics to track campaigns load latency.
  // Load campaigns component via component updater.
  client_->LoadCampaignsComponent(
      base::BindOnce(&CampaignsManager::OnCampaignsComponentLoaded,
                     weak_factory_.GetWeakPtr(), std::move(load_callback)));
}

const Campaign* CampaignsManager::GetCampaignBySlot(Slot slot) const {
  CHECK(campaigns_loaded_)
      << "Getting campaign before campaigns finish loading";
  return matcher_.GetCampaignBySlot(slot);
}

void CampaignsManager::OnCampaignsComponentLoaded(
    base::OnceClosure load_callback,
    const absl::optional<const base::FilePath>& path) {
  if (!path.has_value()) {
    LOG(ERROR) << "Failed to load campaign component.";
    RecordCampaignsManagerError(
        CampaignsManagerError::kCampaignsComponentLoadFail);
    OnCampaignsLoaded(std::move(load_callback), /*campaigns=*/absl::nullopt);
    return;
  }
  // Read the campaigns file from component mounted path.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&ReadCampaignsFile, *path),
      base::BindOnce(&CampaignsManager::OnCampaignsLoaded,
                     weak_factory_.GetWeakPtr(), std::move(load_callback)));
}

void CampaignsManager::OnCampaignsLoaded(
    base::OnceClosure load_callback,
    absl::optional<base::Value::Dict> campaigns_dict) {
  // Load campaigns into campaigns store.
  if (campaigns_dict.has_value()) {
    // Update campaigns store.
    campaigns_store_ = std::move(campaigns_dict.value());
  } else {
    LOG(ERROR) << "No campaign is loaded.";
  }

  // Load campaigns into `CampaignMatcher` for selecting campaigns.
  matcher_.SetCampaigns(GetProactiveCampaigns(&campaigns_store_),
                        GetReactiveCampaigns(&campaigns_store_));
  campaigns_loaded_ = true;

  std::move(load_callback).Run();
  NotifyCampaignsLoaded();
}

void CampaignsManager::NotifyCampaignsLoaded() {
  for (auto& observer : observers_) {
    observer.OnCampaignsLoadCompleted();
  }
}

}  // namespace growth
