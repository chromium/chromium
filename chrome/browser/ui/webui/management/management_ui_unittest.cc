// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui.h"

#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class ManagementUITest : public testing::Test {};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// If the link containing strings will appear as a disclosure add here.
TEST_F(ManagementUITest, VerifyLinksHaveRemovedVersion) {
  static const std::unordered_set<int> kHasNoLinkVersionOrNotApplicable{
      IDS_MANAGEMENT_LOG_UPLOAD_ENABLED, IDS_MANAGEMENT_LEGACY_TECH_REPORT,
      // Not applicable strings follow.
      IDS_MANAGEMENT_PROFILE_REPORTING_LEARN_MORE};

  std::vector<webui::LocalizedString> localized_strings;
  ManagementUI::GetLocalizedStrings(localized_strings, false);

  for (auto i : localized_strings) {
    // Search for link specifier.
    if (l10n_util::GetStringUTF16(i.id).find(u"href=\"") != std::string::npos) {
      EXPECT_TRUE(kHasNoLinkVersionOrNotApplicable.contains(i.id));
    }
  }
}

// All disclosure strings that contain a link should not be included.
TEST_F(ManagementUITest, VerifyLinksRemoved) {
  static const std::unordered_set<int> kLinkNotApplicable{
      IDS_MANAGEMENT_PROFILE_REPORTING_LEARN_MORE};
  std::vector<webui::LocalizedString> localized_strings;
  ManagementUI::GetLocalizedStrings(localized_strings, true);

  for (auto i : localized_strings) {
    // Search for link specifier.
    if (l10n_util::GetStringUTF16(i.id).find(u"href=\"") != std::string::npos) {
      EXPECT_TRUE(kLinkNotApplicable.contains(i.id));
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
