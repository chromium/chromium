// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_FEATURES_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_FEATURES_H_

#include "base/feature_list.h"

namespace translate {

// Controls whether PDF translation is enabled.
BASE_DECLARE_FEATURE(kEnableTranslatePdf);

// Controls whether the simplified Hindi model is used.
BASE_DECLARE_FEATURE(kTranslateSimplifiedHindi);

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_FEATURES_H_
