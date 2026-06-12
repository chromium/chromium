// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_METRICS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_METRICS_H_

#include "components/signin/public/base/signin_deep_link_payload.h"

namespace signin_metrics {

// Initial state of the accounts on the device.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: CrossDeviceInitialState
// LINT.IfChange(CrossDeviceInitialState)
enum class CrossDeviceInitialState {
  kSignedOutTargetAccountOnDevice = 0,
  kSignedOutTargetAccountNotOnDevice = 1,
  kSignedInWithDifferentAccountTargetAccountOnDevice = 2,
  kSignedInWithDifferentAccountTargetAccountNotOnDevice = 3,
  kSignedInWithTargetAccount = 4,
  kFlowForbidden = 5,
  kMaxValue = kFlowForbidden,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:SigninCrossDeviceInitialState)

// Records the raw external entry point ID detected from the cross-device
// sign-in URL.
void RecordUrlDetected(int entry_point_id);

// Records the total number of Google accounts present on the device when the
// cross-device sign-in flow starts.
void RecordInitialAccountsNumber(signin::ExternalEntryPoint entry_point,
                                 int count);

// Records the initial state of the accounts on the device.
void RecordInitialState(signin::ExternalEntryPoint entry_point,
                        CrossDeviceInitialState state);

}  // namespace signin_metrics

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_METRICS_H_
