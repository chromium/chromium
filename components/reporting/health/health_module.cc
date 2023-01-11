// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/health/health_module.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"

namespace reporting {

// Recorder implementation

HealthModule::Recorder::Recorder(scoped_refptr<HealthModule> health_module)
    : health_module_(health_module) {
  if (health_module_) {
    // Time in seconds since Epoch.
    history_.set_timestamp_seconds(base::Time::Now().ToTimeT());
  }
}

HealthModule::Recorder::Recorder(HealthModule::Recorder&& other) = default;

HealthModule::Recorder& HealthModule::Recorder::operator=(
    HealthModule::Recorder&& other) = default;

HealthModule::Recorder::~Recorder() {
  if (health_module_) {
    health_module_->PostHealthRecord(std::move(history_));
  }
}

HealthModule::Recorder::operator bool() const noexcept {
  return health_module_.get() != nullptr;
}

HealthDataHistory& HealthModule::Recorder::operator*() noexcept {
  return history_;
}
HealthDataHistory* HealthModule::Recorder::operator->() noexcept {
  return &history_;
}

// HealthModule implementation

// static
scoped_refptr<HealthModule> HealthModule::Create(
    std::unique_ptr<HealthModuleDelegate> delegate) {
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  return base::WrapRefCounted(
      new HealthModule(std::move(delegate), sequenced_task_runner));
}

HealthModule::HealthModule(std::unique_ptr<HealthModuleDelegate> delegate,
                           scoped_refptr<base::SequencedTaskRunner> task_runner)
    : delegate_(std::move(delegate)), task_runner_(task_runner) {
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&HealthModuleDelegate::Init,
                                                   delegate_->GetWeakPtr()));
}

HealthModule::~HealthModule() {
  // Destruct delegate on the thread (needed it for weak ptr factory).
  task_runner_->DeleteSoon(FROM_HERE, std::move(delegate_));
}

void HealthModule::PostHealthRecord(HealthDataHistory history) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&HealthModuleDelegate::PostHealthRecord,
                                delegate_->GetWeakPtr(), std::move(history)));
}

void HealthModule::GetHealthData(HealthCallback cb) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&HealthModuleDelegate::GetERPHealthData,
                                delegate_->GetWeakPtr(), std::move(cb)));
}
}  // namespace reporting
