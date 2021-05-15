// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PREFS_UTIL_H_
#define COMPONENTS_PRINTING_BROWSER_PREFS_UTIL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

class PrefService;

namespace printing {

// Parse the printing.paper_size_default preference.
absl::optional<gfx::Size> ParsePaperSizeDefault(const PrefService& prefs);

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PREFS_UTIL_H_
