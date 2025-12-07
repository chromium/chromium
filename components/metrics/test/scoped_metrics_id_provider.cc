// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/test/scoped_metrics_id_provider.h"

#include "components/metrics/cloned_install_detector.h"

namespace metrics {

ScopedMachineIdProvider::ScopedMachineIdProvider() {
  ClonedInstallDetector::SetMachineIdProviderForTesting(this);
}

ScopedMachineIdProvider::~ScopedMachineIdProvider() {
  ClonedInstallDetector::SetMachineIdProviderForTesting(nullptr);
}

bool ScopedMachineIdProvider::HasId() const {
  return !machine_id.empty();
}
std::string ScopedMachineIdProvider::GetMachineId() const {
  return machine_id;
}

}  // namespace metrics
