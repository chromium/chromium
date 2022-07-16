// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_safe_mode_constants.h"

namespace variations {

const base::FilePath::CharType kVariationsFilename[] =
    FILE_PATH_LITERAL("Variations");

const char kExtendedSafeModeTrial[] = "ExtendedVariationsSafeMode3";
const char kControlGroup[] = "Control3";
const char kDefaultGroup[] = "Default3";
const char kSignalAndWriteViaFileUtilGroup[] = "SignalAndWriteViaFileUtil3";

}  // namespace variations
