// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_NEUTRINO_LOGGING_UTIL_H_
#define COMPONENTS_METRICS_STRUCTURED_NEUTRINO_LOGGING_UTIL_H_

#include "components/metrics/structured/neutrino_logging.h"
#include "components/prefs/pref_service.h"

// This file is for functions that depend upon //components/metrics.
// Thus, these functions cannot be called by code in //components/metrics.

namespace metrics {
namespace structured {

// Log the location in the code and the client id to the NeutrinoDevices
// structured metrics log. Extract the client id using the local state.
void NeutrinoDevicesLogWithLocalState(PrefService* local_state,
                                      NeutrinoDevicesLocation location);

// Log the enrollment status (managed or unmanged), location in the code and
// the client id to the NeutrinoDevices structured metrics log. Extract the
// client id using the local state.
void NeutrinoDevicesLogEnrollmentWithLocalState(
    PrefService* local_state,
    bool is_managed,
    NeutrinoDevicesLocation location);
}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_NEUTRINO_LOGGING_UTIL_H_
