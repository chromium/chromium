// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_METRICS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_METRICS_H_

#include "components/signin/public/base/signin_deep_link_payload.h"

namespace signin_metrics {

// Records the raw external entry point ID detected from the cross-device
// sign-in URL.
void RecordUrlDetected(int entry_point_id);

// Records the total number of Google accounts present on the device when the
// cross-device sign-in flow starts.
void RecordInitialAccountsNumber(signin::ExternalEntryPoint entry_point,
                                 int count);

}  // namespace signin_metrics

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_METRICS_H_
