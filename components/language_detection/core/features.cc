// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/features.h"

#include "base/feature_list.h"

namespace language_detection::features {

// If enabled, we lazily initiate `TranslateAgent` in
// `ChromeRenderFrameObserver` (crbug/361215212).
BASE_FEATURE(kLazyUpdateTranslateModel,
             "LazyUpdateTranslateModel",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace language_detection::features
