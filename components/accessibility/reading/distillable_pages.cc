// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility/reading/distillable_pages.h"

#include "base/no_destructor.h"

namespace {
std::vector<std::string>& GetDistillableDomainsInternal() {
  static base::NoDestructor<std::vector<std::string>> g_domains;
  return *g_domains;
}
}  // namespace

namespace a11y {

const std::vector<std::string>& GetDistillableDomains() {
  return GetDistillableDomainsInternal();
}

void SetDistillableDomainsForTesting(std::vector<std::string> domains) {
  GetDistillableDomainsInternal().swap(domains);
}

}  // namespace a11y
