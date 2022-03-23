// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_safe_mode_constants.h"

namespace variations {

const base::FilePath::CharType kVariationsFilename[] =
    FILE_PATH_LITERAL("Variations");

const char kExtendedSafeModeTrial[] = "ExtendedVariationsSafeMode5";
const char kControlGroup[] = "Control5";
const char kDefaultGroup[] = "Default5";
const char kEnabledGroup[] = "SignalAndWriteViaFileUtil5";

}  // namespace variations
