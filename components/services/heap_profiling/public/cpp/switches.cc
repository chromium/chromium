// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/switches.h"

namespace heap_profiling {

const char kMemlogMode[] = "memlog";
const char kMemlogModeAll[] = "all";
const char kMemlogModeAllRenderers[] = "all-renderers";
const char kMemlogModeBrowser[] = "browser";
const char kMemlogModeGpu[] = "gpu";
const char kMemlogModeManual[] = "manual";
const char kMemlogModeMinimal[] = "minimal";
const char kMemlogModeRendererSampling[] = "renderer-sampling";
const char kMemlogModeUtilityAndBrowser[] = "utility-and-browser";
const char kMemlogModeUtilitySampling[] = "utility-sampling";

const char kMemlogSamplingRate[] = "memlog-sampling-rate";
const char kMemlogSamplingRate10KB[] = "10000";
const char kMemlogSamplingRate50KB[] = "50000";
const char kMemlogSamplingRate100KB[] = "100000";
const char kMemlogSamplingRate500KB[] = "500000";
const char kMemlogSamplingRate1MB[] = "1000000";
const char kMemlogSamplingRate5MB[] = "5000000";

const char kMemlogStackMode[] = "memlog-stack-mode";
const char kMemlogStackModeMixed[] = "mixed";
const char kMemlogStackModeNative[] = "native";
const char kMemlogStackModeNativeWithThreadNames[] = "native-with-thread-names";
const char kMemlogStackModePseudo[] = "pseudo";

}  // namespace heap_profiling
