// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility/reading/distillable_urls.h"

#include "base/no_destructor.h"

namespace a11y {

const std::vector<GURL>& GetDistillableURLs() {
  static const base::NoDestructor<std::vector<GURL>> g_urls;
  return *g_urls;
}

}  // namespace a11y
