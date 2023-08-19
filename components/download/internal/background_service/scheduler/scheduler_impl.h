// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_SCHEDULER_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_SCHEDULER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/download/internal/background_service/scheduler/scheduler.h"

#include <map>
#include <vector>

#include "components/download/internal/background_service/entry.h"

namespace download {

class ClientSet;
class TaskScheduler;
struct Configuration;

// Scheduler implementation that
// 1. Creates platform background task based on the states of download entries.
// 2. Polls the next entry to be processed by the service mainly according to
// scheduling parameters and current device status.
//
// Provides load balancing between download clients using the service.
class SchedulerImpl : public Scheduler {
 public:
  SchedulerImpl(TaskScheduler* task_scheduler,
                Configuration* config,
                const ClientSet* clients);
  SchedulerImpl(TaskScheduler* task_scheduler,
                Configuration* config,
                const std::vector<DownloadClient>& clients);

  SchedulerImpl(const SchedulerImpl&) = delete;
  SchedulerImpl& operator=(const SchedulerImpl&) = delete;

  ~SchedulerImpl() override;

  // Scheduler implementation.
  void Reschedule(const Model::EntryList& entries) override;
  Entry* Next(const Model::EntryList& entries,
              const DeviceStatus& device_status) override;

 private:
  // Finds a candidate for each download client to be processed next by the
  // service.
  // The candidates are selected based on scheduling parameters and current
  // device status.
  std::map<DownloadClient, Entry*> FindCandidates(
      const Model::EntryList& entries,
      const DeviceStatus& device_status);

  // Used to create platform dependent background tasks.
  raw_ptr<TaskScheduler, AcrossTasksDanglingUntriaged> task_scheduler_;

  // Download service configuration.
  raw_ptr<Configuration, DanglingUntriaged> config_;

  // List of all download client id, used in round robin load balancing.
  // Downloads will be delivered to clients with incremental order based on
  // the index of this list.
  const std::vector<DownloadClient> download_clients_;

  // The index of the current client.
  // See |download_clients_|.
  size_t current_client_index_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_SCHEDULER_IMPL_H_
