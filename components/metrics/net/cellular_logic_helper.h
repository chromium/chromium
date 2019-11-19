// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_NET_CELLULAR_LOGIC_HELPER_H_
#define COMPONENTS_METRICS_NET_CELLULAR_LOGIC_HELPER_H_

#include "base/time/time.h"

namespace metrics {

// Returns UMA log upload interval based on OS. If
// |use_cellular_upload_interval| is true, this returns an interval suitable for
// metered cellular connections. Otherwise, this returns an interval suitable
// for unmetered (ex. WiFi) connections.
base::TimeDelta GetUploadInterval(bool use_cellular_upload_interval);

// Returns true if current connection type is cellular and the platform supports
// using a separate interval for cellular connections (at the moment, this is
// supported for OS_ANDROID and OS_IOS).
bool ShouldUseCellularUploadInterval();

}  // namespace metrics

#endif  // COMPONENTS_METRICS_NET_CELLULAR_LOGIC_HELPER_H_
