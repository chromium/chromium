// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_ETW_EXPORT_WIN_H_
#define COMPONENTS_TRACING_COMMON_ETW_EXPORT_WIN_H_

#include "components/tracing/tracing_export.h"

namespace tracing {

// Enables exporting of track events to ETW in the current process.
TRACING_EXPORT void EnableETWExport();

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_ETW_EXPORT_WIN_H_
