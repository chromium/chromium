// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_TEST_SCOPED_METRICS_ID_PROVIDER_H_
#define COMPONENTS_METRICS_TEST_SCOPED_METRICS_ID_PROVIDER_H_

#include "components/metrics/machine_id_provider.h"

namespace metrics {

// Helper class to automatically set and reset a global `MachineIdProvider`
// instance in tests, see
// `ClonedInstallDetector::SetMachineIdProviderForTesting`.
class ScopedMachineIdProvider : public MachineIdProvider {
 public:
  ScopedMachineIdProvider();
  ~ScopedMachineIdProvider() override;

  bool HasId() const override;
  std::string GetMachineId() const override;

  std::string machine_id;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_TEST_SCOPED_METRICS_ID_PROVIDER_H_
