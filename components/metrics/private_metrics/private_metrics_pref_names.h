// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_PREF_NAMES_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_PREF_NAMES_H_

namespace metrics::private_metrics::prefs {

// Preference which stores serialized private metrics logs to be uploaded.
inline constexpr char kUnsentLogStoreName[] = "private_metrics.persistent_logs";

// Preference which stores client_id for PUMA Regional Capabilities.
inline constexpr char kPumaRcClientId[] = "private_metrics.puma.client_id.rc";

}  // namespace metrics::private_metrics::prefs

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_METRICS_PREF_NAMES_H_
