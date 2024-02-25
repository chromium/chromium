// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_hints_component_update_listener.h"

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace optimization_guide {

// static
OptimizationHintsComponentUpdateListener*
OptimizationHintsComponentUpdateListener::GetInstance() {
  static base::NoDestructor<OptimizationHintsComponentUpdateListener> service;
  return service.get();
}

OptimizationHintsComponentUpdateListener::
    OptimizationHintsComponentUpdateListener() = default;
OptimizationHintsComponentUpdateListener::
    ~OptimizationHintsComponentUpdateListener() = default;

void OptimizationHintsComponentUpdateListener::AddObserver(
    OptimizationHintsComponentObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);

  if (hints_component_info_) {
    observer->OnHintsComponentAvailable(*hints_component_info_);
  }
}

void OptimizationHintsComponentUpdateListener::RemoveObserver(
    OptimizationHintsComponentObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void OptimizationHintsComponentUpdateListener::MaybeUpdateHintsComponent(
    const HintsComponentInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(info.version.IsValid());
  DCHECK(!info.path.empty());

  base::UmaHistogramSparse(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion2",
      info.version.components()[0]);

  // Do not update the component if the version isn't newer. This differs from
  // the check in ComponentInstaller::InstallHelper(), because this rejects
  // version equality, whereas InstallHelper() accepts it.
  if (hints_component_info_ &&
      hints_component_info_->version.CompareTo(info.version) >= 0) {
    return;
  }

  hints_component_info_.emplace(info.version, info.path);
  for (auto& observer : observers_) {
    observer.OnHintsComponentAvailable(*hints_component_info_);
  }
}

void OptimizationHintsComponentUpdateListener::ResetStateForTesting() {
  hints_component_info_ = std::nullopt;
}

}  // namespace optimization_guide
