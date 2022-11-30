// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_PREF_NAMES_H_
#define COMPONENTS_TRACING_COMMON_PREF_NAMES_H_

#include "base/component_export.h"

class PrefRegistrySimple;

namespace tracing {

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
extern const char kBackgroundTracingSessionState[];

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_PREF_NAMES_H_
