// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SHUTDOWN_H_
#define COMPONENTS_METRICS_METRICS_SHUTDOWN_H_

#include "components/prefs/pref_service.h"

namespace metrics {

// Does some final work before the browser shuts down. Note that this may not be
// called on all platforms (e.g. on Android, where there is no clean shutdown
// path).
void Shutdown(PrefService* local_state);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SHUTDOWN_H_
