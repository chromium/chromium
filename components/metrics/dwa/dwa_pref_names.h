// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DWA_DWA_PREF_NAMES_H_
#define COMPONENTS_METRICS_DWA_DWA_PREF_NAMES_H_

#include "base/component_export.h"

namespace metrics::dwa::prefs {

// Preference which stores serialized DWA logs to be uploaded.
COMPONENT_EXPORT(DWA)
extern const char kUnsentLogStoreName[];

// Client ID which changes daily. Note that this is a local state pref which is
// shared between profiles.
COMPONENT_EXPORT(DWA)
extern const char kDwaClientId[];

// Date when `kDwaClientId` was last updated.
COMPONENT_EXPORT(DWA)
extern const char kDwaClientIdLastUpdated[];

}  // namespace metrics::dwa::prefs

#endif  // COMPONENTS_METRICS_DWA_DWA_PREF_NAMES_H_
