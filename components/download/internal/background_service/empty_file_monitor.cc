// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/empty_file_monitor.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace download {

EmptyFileMonitor::EmptyFileMonitor() {}

EmptyFileMonitor::~EmptyFileMonitor() = default;

void EmptyFileMonitor::Initialize(InitCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /* success */));
}

void EmptyFileMonitor::DeleteUnknownFiles(
    const Model::EntryList& known_entries,
    const std::vector<DriverEntry>& known_driver_entries,
    base::OnceClosure completion_callback) {}

void EmptyFileMonitor::CleanupFilesForCompletedEntries(
    const Model::EntryList& entries,
    base::OnceClosure completion_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                std::move(completion_callback));
}

void EmptyFileMonitor::DeleteFiles(
    const std::set<base::FilePath>& files_to_remove,
    stats::FileCleanupReason reason) {}

void EmptyFileMonitor::HardRecover(InitCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /* success */));
}

}  // namespace download
