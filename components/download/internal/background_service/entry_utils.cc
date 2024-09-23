// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/entry_utils.h"

#include <algorithm>

#include "components/download/internal/background_service/download_driver.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/public/background_service/download_metadata.h"

namespace download {
namespace util {

uint32_t GetNumberOfLiveEntriesForClient(DownloadClient client,
                                         const std::vector<Entry*>& entries) {
  uint32_t count = 0;
  for (auto* entry : entries)
    if (entry->client == client && entry->state != Entry::State::COMPLETE)
      count++;

  return count;
}

std::map<DownloadClient, std::vector<DownloadMetaData>>
MapEntriesToMetadataForClients(const std::set<DownloadClient>& clients,
                               const std::vector<Entry*>& entries,
                               DownloadDriver* driver) {
  std::map<DownloadClient, std::vector<DownloadMetaData>> categorized;

  for (auto* entry : entries) {
    DownloadClient client = entry->client;
    if (clients.find(client) == clients.end())
      client = DownloadClient::INVALID;

    categorized[client].push_back(BuildDownloadMetaData(entry, driver));
  }

  return categorized;
}

Criteria GetSchedulingCriteria(const Model::EntryList& entries,
                               int optimal_battery_percentage) {
  Criteria criteria(optimal_battery_percentage);

  // The scheduling criteria can only become less strict, from requiring battery
  // charging to not requiring charging, from requiring unmetered network to
  // any network. Optimal battery level can only decrease.
  for (auto* const entry : entries) {
    DCHECK(entry);
    const SchedulingParams& scheduling_params = entry->scheduling_params;
    switch (scheduling_params.battery_requirements) {
      case SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE:
        criteria.requires_battery_charging = false;
        criteria.optimal_battery_percentage =
            std::min(criteria.optimal_battery_percentage,
                     DeviceStatus::kBatteryPercentageAlwaysStart);
        break;
      case SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE:
        criteria.requires_battery_charging = false;
        criteria.optimal_battery_percentage = std::min(
            criteria.optimal_battery_percentage, optimal_battery_percentage);
        break;
      case SchedulingParams::BatteryRequirements::BATTERY_CHARGING:
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    if (scheduling_params.network_requirements ==
        SchedulingParams::NetworkRequirements::NONE) {
      criteria.requires_unmetered_network = false;
    }
  }
  return criteria;
}

bool EntryBetterThan(const Entry& lhs, const Entry& rhs) {
  if (lhs.scheduling_params.priority != rhs.scheduling_params.priority)
    return lhs.scheduling_params.priority > rhs.scheduling_params.priority;

  if (lhs.scheduling_params.cancel_time != rhs.scheduling_params.cancel_time) {
    return lhs.scheduling_params.cancel_time <
           rhs.scheduling_params.cancel_time;
  }

  return lhs.create_time < rhs.create_time;
}

DownloadMetaData BuildDownloadMetaData(Entry* entry, DownloadDriver* driver) {
  DCHECK(entry);
  DownloadMetaData meta_data;
  meta_data.guid = entry->guid;
  meta_data.paused = entry->state == Entry::State::PAUSED;
  if (entry->state == Entry::State::COMPLETE) {
    meta_data.completion_info =
        CompletionInfo(entry->target_file_path, entry->bytes_downloaded,
                       entry->url_chain, entry->response_headers);
    meta_data.completion_info->custom_data = entry->custom_data;

    // If the download is completed, the |current_size| needs to pull from entry
    // since the history db record has been deleted.
    meta_data.current_size = entry->bytes_downloaded;
    return meta_data;
  }

  auto driver_entry = driver->Find(entry->guid);
  if (driver_entry)
    meta_data.current_size = driver_entry->bytes_downloaded;
  return meta_data;
}

}  // namespace util
}  // namespace download
