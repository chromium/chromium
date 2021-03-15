// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_internal_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "chrome/updater/check_for_updates_task.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/util.h"
#include "components/prefs/pref_service.h"

namespace updater {

UpdateServiceInternalImpl::UpdateServiceInternalImpl(
    scoped_refptr<updater::Configurator> config)
    : config_(config) {}

void UpdateServiceInternalImpl::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto task = base::MakeRefCounted<CheckForUpdatesTask>(
      config_, base::BindOnce(&UpdateServiceInternalImpl::TaskDone, this,
                              std::move(callback)));
  // Queues the task to be run. If no other tasks are running, runs the task.
  tasks_.push(task);
  if (tasks_.size() == 1)
    RunNextTask();
}

void UpdateServiceInternalImpl::InitializeUpdateService(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

void UpdateServiceInternalImpl::TaskDone(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
  tasks_.pop();
  RunNextTask();
}

void UpdateServiceInternalImpl::RunNextTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!tasks_.empty()) {
    tasks_.front()->Run();
  }
}

void UpdateServiceInternalImpl::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefsCommitPendingWrites(config_->GetPrefService());
}

UpdateServiceInternalImpl::~UpdateServiceInternalImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_->GetPrefService()->SchedulePendingLossyWrites();
}

}  // namespace updater
