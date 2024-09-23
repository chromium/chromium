// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_READING_DISTILLABLE_PAGES_H_
#define COMPONENTS_ACCESSIBILITY_READING_DISTILLABLE_PAGES_H_

#include <string>
#include <vector>

namespace a11y {
// A list of domains which are known to contain distillable pages (i.e.
// articles). If the user visits one of these sites and that site has a filename
// (i.e. it is not a home page) we will show the IPH. This implementation was
// chosen in order to show the IPH on pages where the distillation model has the
// best chance of success.
const std::vector<std::string>& GetDistillableDomains();

// Test method to set domains for testing.
void SetDistillableDomainsForTesting(std::vector<std::string> domains);
}  // namespace a11y

#endif  // COMPONENTS_ACCESSIBILITY_READING_DISTILLABLE_PAGES_H_
