// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/build_state.h"

#include <tuple>
#include <utility>

#include "base/observer_list.h"
#include "chrome/browser/upgrade_detector/build_state_observer.h"

BuildState::BuildState() = default;

BuildState::~BuildState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BuildState::SetUpdate(
    UpdateType update_type,
    const base::Version& installed_version,
    const std::optional<base::Version>& critical_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(update_type != UpdateType::kNone ||
         (!installed_version.IsValid() && !critical_version.has_value()));

  std::optional<base::Version> new_installed_version;
  if (installed_version.IsValid())
    new_installed_version = installed_version;

  // Update state and notify observers only in case of a change in state.
  if (std::tie(update_type_, installed_version_, critical_version_) !=
      std::tie(update_type, new_installed_version, critical_version)) {
    update_type_ = update_type;
    installed_version_ = std::move(new_installed_version);
    critical_version_ = critical_version;
    NotifyObserversOnUpdate();
  }
}

void BuildState::AddObserver(BuildStateObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void BuildState::RemoveObserver(const BuildStateObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

bool BuildState::HasObserver(const BuildStateObserver* observer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return observers_.HasObserver(observer);
}

void BuildState::NotifyObserversOnUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.OnUpdate(this);
}
