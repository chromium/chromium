// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/l10n_string_util.h"

#include <string>

#include "build/branding_buildflags.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

// Test that each mode-specific string has a distinct value among the brand's
// install modes.
TEST(GetLocalizedStringTest, DistinctStrings) {
  static constexpr int kStringIds[] = {
// Generate the list of mode-specific string IDs.
#define HANDLE_MODE_STRING(id, ...) id,
      DO_MODE_STRINGS
#undef HANDLE_MODE_STRING
  };
  for (int string_id : kStringIds) {
    SCOPED_TRACE(testing::Message() << "message id: " << string_id);
    std::set<std::wstring> the_strings;
    for (int mode_index = 0; mode_index < install_static::NUM_INSTALL_MODES;
         ++mode_index) {
      SCOPED_TRACE(testing::Message() << "install mode index: " << mode_index);
      install_static::ScopedInstallDetails install_details(false, mode_index);
      std::wstring the_string = GetLocalizedString(string_id);
      ASSERT_FALSE(the_string.empty());
      EXPECT_TRUE(the_strings.insert(the_string).second)
          << the_string << " is found in more than one install mode.";
    }
  }
}

TEST(GetLocalizedStringFTest, ElevationServiceDescription) {
  constexpr std::wstring_view placeholder = L"$1";
  const std::wstring replacement = L"foobar";

  std::wstring string_with_placeholder =
      GetLocalizedString(IDS_ELEVATION_SERVICE_DESCRIPTION_BASE);
  for (std::wstring::size_type n = 0;
       (n = string_with_placeholder.find(placeholder, n)) != std::wstring::npos;
       n += replacement.size()) {
    string_with_placeholder.replace(n, placeholder.size(), replacement);
  }

  ASSERT_EQ(GetLocalizedStringF(IDS_ELEVATION_SERVICE_DESCRIPTION_BASE,
                                {replacement}),
            string_with_placeholder);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Test that the mode-specific string mappings are correct for Google Chrome
// builds.
TEST(GetBaseMessageIdForMode, GoogleStringIds) {
  // The list of string ids that are mapped based on the install mode. This
  // matches the top-level identifiers in create_string_rc.py's
  // MODE_SPECIFIC_STRINGS data structure.
  std::vector<int> input_ids({IDS_APP_SHORTCUTS_SUBDIR_NAME_BASE,
                              IDS_INBOUND_MDNS_RULE_DESCRIPTION_BASE,
                              IDS_INBOUND_MDNS_RULE_NAME_BASE,
                              IDS_PRODUCT_NAME_BASE});

  // A map from an install mode index to its mode-specific string identifiers.
  std::map<int, std::vector<int>> mode_to_strings;
  mode_to_strings[install_static::STABLE_INDEX] = std::vector<int>(
      {IDS_APP_SHORTCUTS_SUBDIR_NAME_BASE,
       IDS_INBOUND_MDNS_RULE_DESCRIPTION_BASE, IDS_INBOUND_MDNS_RULE_NAME_BASE,
       IDS_PRODUCT_NAME_BASE});
  mode_to_strings[install_static::BETA_INDEX] = std::vector<int>(
      {IDS_APP_SHORTCUTS_SUBDIR_NAME_BETA_BASE,
       IDS_INBOUND_MDNS_RULE_DESCRIPTION_BETA_BASE,
       IDS_INBOUND_MDNS_RULE_NAME_BETA_BASE, IDS_SHORTCUT_NAME_BETA_BASE});
  mode_to_strings[install_static::DEV_INDEX] = std::vector<int>(
      {IDS_APP_SHORTCUTS_SUBDIR_NAME_DEV_BASE,
       IDS_INBOUND_MDNS_RULE_DESCRIPTION_DEV_BASE,
       IDS_INBOUND_MDNS_RULE_NAME_DEV_BASE, IDS_SHORTCUT_NAME_DEV_BASE});
  mode_to_strings[install_static::CANARY_INDEX] = std::vector<int>(
      {IDS_APP_SHORTCUTS_SUBDIR_NAME_CANARY_BASE,
       IDS_INBOUND_MDNS_RULE_DESCRIPTION_CANARY_BASE,
       IDS_INBOUND_MDNS_RULE_NAME_CANARY_BASE, IDS_SXS_SHORTCUT_NAME_BASE});

  // Run through all install modes, checking that the mode-specific strings are
  // mapped properly by GetBaseMessageIdForMode.
  ASSERT_EQ(static_cast<size_t>(install_static::NUM_INSTALL_MODES),
            mode_to_strings.size());
  for (int mode_index = 0; mode_index < install_static::NUM_INSTALL_MODES;
       ++mode_index) {
    SCOPED_TRACE(testing::Message() << "install mode index: " << mode_index);
    ASSERT_EQ(1U, mode_to_strings.count(mode_index));
    const auto& mode_strings = mode_to_strings[mode_index];
    ASSERT_EQ(mode_strings.size(), input_ids.size());

    install_static::ScopedInstallDetails install_details(false, mode_index);
    for (size_t i = 0; i < input_ids.size(); ++i)
      EXPECT_EQ(mode_strings[i], GetBaseMessageIdForMode(input_ids[i]));
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace installer
