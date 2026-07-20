// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/public/features.h"

namespace notebooks::features {

BASE_FEATURE(kNotebooks, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string, kNotebookHomeURL, &kNotebooks, "about:blank");

}  // namespace notebooks::features
