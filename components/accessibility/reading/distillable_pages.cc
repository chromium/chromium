// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility/reading/distillable_pages.h"

#include "base/no_destructor.h"

namespace a11y {

const std::vector<std::string>& GetDistillableDomains() {
  static const base::NoDestructor<std::vector<std::string>> g_domains;
  return *g_domains;
}

}  // namespace a11y
