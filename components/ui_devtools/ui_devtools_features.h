// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_UI_DEVTOOLS_FEATURES_H_
#define COMPONENTS_UI_DEVTOOLS_UI_DEVTOOLS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace ui_devtools {

// Enables UI debugging tools to send synthetic events.
// This is used by the telemetry benchmarking tools only.
COMPONENT_EXPORT(UI_DEVTOOLS_FEATURES)
BASE_DECLARE_FEATURE(kUIDebugToolsEnableSyntheticEvents);

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_UI_DEVTOOLS_FEATURES_H_
