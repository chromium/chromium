// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_safe_mode_constants.h"

namespace variations {

const base::FilePath::CharType kVariationsFilename[] =
    FILE_PATH_LITERAL("Variations");

const char kExtendedSafeModeTrial[] = "ExtendedVariationsSafeMode";
const char kControlGroup[] = "Control2";
const char kDefaultGroup[] = "Default2";
const char kSignalAndWriteSynchronouslyViaPrefServiceGroup[] =
    "SignalAndWriteSynchronouslyViaPrefService";
const char kSignalAndWriteViaFileUtilGroup[] = "SignalAndWriteViaFileUtil";
const char kWriteSynchronouslyViaPrefServiceGroup[] =
    "WriteSynchronouslyViaPrefService";

}  // namespace variations
