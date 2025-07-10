// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_ntp_promos.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Checks metadata and returns a list of errors.
std::vector<std::string> CheckMetadata(
    const user_education::Metadata& metadata) {
  std::vector<std::string> errors;
  if (!metadata.launch_milestone) {
    errors.emplace_back("launch milestone");
  }
  if (metadata.owners.empty()) {
    errors.emplace_back("owners list");
  }
  if (metadata.additional_description.empty()) {
    errors.emplace_back("description");
  }
  return errors;
}

}  // namespace

TEST(BrowserNtpPromosTest, CheckMetadata) {
  user_education::NtpPromoRegistry registry;
  MaybeRegisterNtpPromos(registry);
  std::ostringstream oss;
  bool failed = false;
  for (const auto& identifier : registry.GetNtpPromoIdentifiers()) {
    const auto* spec = registry.GetNtpPromoSpecification(identifier);
    const auto errors = CheckMetadata(spec->metadata());
    if (!errors.empty()) {
      failed = true;
      oss << "\nNTP Promo " << identifier
          << " is missing metadata: " << base::JoinString(errors, ", ");
    }
  }
  EXPECT_FALSE(failed) << "\"New\" Badges missing metadata:" << oss.str();
}
