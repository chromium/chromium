// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_READING_DISTILLABLE_URLS_H_
#define COMPONENTS_ACCESSIBILITY_READING_DISTILLABLE_URLS_H_

#include <vector>

#include "url/gurl.h"

namespace a11y {
// A list of urls which are known to contain distillable pages (i.e. articles).
// If the user visits one of these sites and that site has a filename (i.e. it
// is not a home page) we will show the IPH. This implementation was chosen in
// order to show the IPH on pages where the distillation model has the best
// chance of success.
const std::vector<GURL>& GetDistillableURLs();
}  // namespace a11y

#endif  // COMPONENTS_ACCESSIBILITY_READING_DISTILLABLE_URLS_H_
