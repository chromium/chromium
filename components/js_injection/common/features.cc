// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/common/features.h"

namespace js_injection {

BASE_DECLARE_FEATURE(kArrayBufferJsToBrowser);

BASE_FEATURE(kArrayBufferJsToBrowser,
             "JsInjectionArrayBufferJsToBrowser",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsJsToBrowserArrayBufferSupported() {
  return base::FeatureList::IsEnabled(kArrayBufferJsToBrowser);
}

}  // namespace js_injection
