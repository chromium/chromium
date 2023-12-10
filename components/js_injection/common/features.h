// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_COMMON_FEATURES_H_
#define COMPONENTS_JS_INJECTION_COMMON_FEATURES_H_

#include "base/feature_list.h"

namespace js_injection {

// Feature to enable ArrayBuffer support in JS to Browser messaging.
BASE_DECLARE_FEATURE(kArrayBufferJsToBrowser);

bool IsJsToBrowserArrayBufferSupported();

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_COMMON_FEATURES_H_
