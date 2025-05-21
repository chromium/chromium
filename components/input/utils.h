// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_UTILS_H_
#define COMPONENTS_INPUT_UTILS_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

namespace input {

// The class contains utility methods related to input handling.
class COMPONENT_EXPORT(INPUT) InputUtils {
 public:
  // Check whether input thandling on Viz is supported. Currently it's supported
  // only on Android V+ which contain security fix for `CVE-2025-0097`.
  static bool IsTransferInputToVizSupported();
#if BUILDFLAG(IS_ANDROID)
 private:
  FRIEND_TEST_ALL_PREFIXES(UtilsTest,
                           InputToVizNotSupportedOnOlderSecurityPatchLevel);
  static bool HasSecurityUpdate(const std::string& security_patch);

  // Checks if other static member variables has been initialized.
  static bool initialized_;

  // Caches the result of whether security patch contains the fix for
  // `CVE-2025-0097`. We are caching this instead of parsing the security patch
  // strings each time for comparison.
  static bool has_security_update_;
#endif
};

perfetto::protos::pbzero::ChromeLatencyInfo2::InputType InputEventTypeToProto(
    blink::WebInputEvent::Type event_type);

perfetto::protos::pbzero::ChromeLatencyInfo2::InputResultState
InputEventResultStateToProto(blink::mojom::InputEventResultState result_state);

}  // namespace input

#endif  // COMPONENTS_INPUT_UTILS_H_
