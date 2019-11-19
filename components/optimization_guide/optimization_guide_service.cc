// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_service.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"

namespace optimization_guide {

OptimizationGuideService::OptimizationGuideService(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread_task_runner)
    : ui_thread_task_runner_(ui_thread_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

OptimizationGuideService::~OptimizationGuideService() {}

void OptimizationGuideService::AddObserver(
    OptimizationGuideServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
  if (hints_component_info_) {
    observer->OnHintsComponentAvailable(*hints_component_info_);
  }
}

void OptimizationGuideService::RemoveObserver(
    OptimizationGuideServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void OptimizationGuideService::MaybeUpdateHintsComponent(
    const HintsComponentInfo& info) {
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OptimizationGuideService::MaybeUpdateHintsComponentOnUIThread,
          weak_ptr_factory_.GetWeakPtr(), info));
}

void OptimizationGuideService::MaybeUpdateHintsComponentOnUIThread(
    const HintsComponentInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(info.version.IsValid());
  DCHECK(!info.path.empty());

  // Do not update the component if the version isn't newer. This differs from
  // the check in ComponentInstaller::InstallHelper(), because this rejects
  // version equality, whereas InstallHelper() accepts it.
  if (hints_component_info_ &&
      hints_component_info_->version.CompareTo(info.version) >= 0) {
    return;
  }

  base::UmaHistogramSparse(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion",
      info.version.components()[0]);

  hints_component_info_.emplace(info.version, info.path);
  for (auto& observer : observers_) {
    observer.OnHintsComponentAvailable(*hints_component_info_);
  }
}

}  // namespace optimization_guide
