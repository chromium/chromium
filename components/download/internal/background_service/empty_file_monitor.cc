// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/empty_file_monitor.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace download {

EmptyFileMonitor::EmptyFileMonitor() {}

EmptyFileMonitor::~EmptyFileMonitor() = default;

void EmptyFileMonitor::Initialize(const InitCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindRepeating(callback, true /* success */));
}

void EmptyFileMonitor::DeleteUnknownFiles(
    const Model::EntryList& known_entries,
    const std::vector<DriverEntry>& known_driver_entries) {}

void EmptyFileMonitor::CleanupFilesForCompletedEntries(
    const Model::EntryList& entries,
    const base::RepeatingClosure& completion_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindRepeating(completion_callback));
}

void EmptyFileMonitor::DeleteFiles(
    const std::set<base::FilePath>& files_to_remove,
    stats::FileCleanupReason reason) {}

void EmptyFileMonitor::HardRecover(const InitCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindRepeating(callback, true /* success */));
}

}  // namespace download
