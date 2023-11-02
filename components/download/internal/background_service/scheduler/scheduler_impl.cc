// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/scheduler/scheduler_impl.h"

#include "components/download/internal/background_service/client_set.h"
#include "components/download/internal/background_service/config.h"
#include "components/download/internal/background_service/entry_utils.h"
#include "components/download/internal/background_service/scheduler/device_status.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/task/task_scheduler.h"

namespace download {

namespace {

// Returns a vector of elements contained in the |set|.
template <typename T>
std::vector<T> ToList(const std::set<T>& set) {
  std::vector<T> list;
  for (const auto& element : set) {
    list.push_back(element);
  }
  return list;
}

}  // namespace

SchedulerImpl::SchedulerImpl(TaskScheduler* task_scheduler,
                             Configuration* config,
                             const ClientSet* clients)
    : SchedulerImpl(task_scheduler,
                    config,
                    ToList<DownloadClient>(clients->GetRegisteredClients())) {}

SchedulerImpl::SchedulerImpl(TaskScheduler* task_scheduler,
                             Configuration* config,
                             const std::vector<DownloadClient>& clients)
    : task_scheduler_(task_scheduler),
      config_(config),
      download_clients_(clients),
      current_client_index_(0) {
  DCHECK(task_scheduler_);
}

SchedulerImpl::~SchedulerImpl() = default;

void SchedulerImpl::Reschedule(const Model::EntryList& entries) {
  if (entries.empty()) {
    task_scheduler_->CancelTask(DownloadTaskType::DOWNLOAD_TASK);
    return;
  }

  // TODO(xingliu): Support NetworkRequirements::OPTIMISTIC.

  Criteria criteria = util::GetSchedulingCriteria(
      entries, config_->download_battery_percentage);
  task_scheduler_->ScheduleTask(
      DownloadTaskType::DOWNLOAD_TASK, criteria.requires_unmetered_network,
      criteria.requires_battery_charging, criteria.optimal_battery_percentage,
      base::saturated_cast<long>(config_->window_start_time.InSeconds()),
      base::saturated_cast<long>(config_->window_end_time.InSeconds()));
}

Entry* SchedulerImpl::Next(const Model::EntryList& entries,
                           const DeviceStatus& device_status) {
  std::map<DownloadClient, Entry*> candidates =
      FindCandidates(entries, device_status);

  Entry* entry = nullptr;
  size_t index = current_client_index_;

  // Finds the next entry to download.
  for (size_t i = 0; i < download_clients_.size(); ++i) {
    DownloadClient client =
        download_clients_[(index + i) % download_clients_.size()];
    Entry* candidate = candidates[client];

    // Some clients may have no entries, continue to check other clients.
    if (!candidate)
      continue;

    bool ui_priority =
        candidate->scheduling_params.priority == SchedulingParams::Priority::UI;

    // Records the first available candidate. Keep iterating to see if there
    // are UI priority entries for other clients.
    if (!entry || ui_priority) {
      entry = candidate;

      // Load balancing between clients.
      current_client_index_ = (index + i + 1) % download_clients_.size();

      // UI priority entry will be processed immediately.
      if (ui_priority)
        break;
    }
  }
  return entry;
}

std::map<DownloadClient, Entry*> SchedulerImpl::FindCandidates(
    const Model::EntryList& entries,
    const DeviceStatus& device_status) {
  std::map<DownloadClient, Entry*> candidates;

  if (entries.empty())
    return candidates;

  for (auto* const entry : entries) {
    DCHECK(entry);
    const SchedulingParams& current_params = entry->scheduling_params;

    // Every download needs to pass the state and device status check.
    if (entry->state != Entry::State::AVAILABLE ||
        !device_status
             .MeetsCondition(current_params,
                             config_->download_battery_percentage)
             .MeetsRequirements()) {
      continue;
    }

    // Find the most appropriate download based on priority and cancel time.
    Entry* candidate = candidates[entry->client];
    if (!candidate || util::EntryBetterThan(*entry, *candidate)) {
      candidates[entry->client] = entry;
    }
  }

  return candidates;
}

}  // namespace download
