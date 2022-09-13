// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/health/health_module.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "components/reporting/util/file.h"

namespace reporting {

namespace {
constexpr char kHistoryFileBasename[] = "health_info_";
}

HealthModule::~HealthModule() = default;

// static
scoped_refptr<HealthModule> HealthModule::Create(
    const base::FilePath& directory) {
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  return base::WrapRefCounted(
      new HealthModule(directory, sequenced_task_runner));
}

HealthModule::HealthModule(const base::FilePath& directory,
                           scoped_refptr<base::SequencedTaskRunner> task_runner)
    : delegate_(std::make_unique<HealthModuleDelegate>(directory,
                                                       kHistoryFileBasename,
                                                       max_history_bytes_)),
      task_runner_(task_runner) {
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&HealthModuleDelegate::Init,
                                                   delegate_->GetWeakPtr()));
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
