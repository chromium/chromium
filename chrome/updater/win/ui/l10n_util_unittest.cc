// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/l10n_util.h"

#include <string>

#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {
constexpr int kUpdaterStringIds[] = {
#define HANDLE_STRING(id, ...) id,
    DO_STRING_MAPPING
#undef HANDLE_STRING
};
}  // namespace

TEST(UpdaterL10NUtilTest, GetLocalizedStrings) {
  for (int id : kUpdaterStringIds) {
    ASSERT_FALSE(GetLocalizedString(id).empty());
  }
}

TEST(UpdaterL10NUtilTest, GetLocalizedStringsFormatted) {
  {
    const std::wstring& replacement = L"foobar";
    for (int id : kUpdaterStringIds) {
      std::wstring localized_string_unformatted = GetLocalizedString(id);
      std::wstring localized_string_formatted =
          GetLocalizedStringF(id, replacement);

      ASSERT_FALSE(localized_string_unformatted.empty());
      ASSERT_FALSE(localized_string_formatted.empty());
      if (localized_string_unformatted.find(replacement) != std::string::npos) {
        ASSERT_NE(localized_string_formatted.find(replacement),
                  std::string::npos);
      }
      ASSERT_EQ(localized_string_formatted.find(L"$1"), std::string::npos);
    }
  }

  {
    const std::vector<std::wstring> replacements = {L"foobar", L"replacement",
                                                    L"str"};
    for (int id : kUpdaterStringIds) {
      std::wstring localized_string_unformatted = GetLocalizedString(id);
      std::wstring localized_string_formatted =
          GetLocalizedStringF(id, replacements);
      ASSERT_FALSE(localized_string_unformatted.empty());
      ASSERT_FALSE(localized_string_formatted.empty());

      for (size_t i = 0; i < replacements.size(); ++i) {
        if (localized_string_unformatted.find(replacements[i]) !=
            std::string::npos) {
          ASSERT_NE(localized_string_formatted.find(replacements[i]),
                    std::string::npos);
        }
        std::wstring replacement_str = L"$";
        replacement_str += (L'1' + i);
        ASSERT_EQ(localized_string_formatted.find(replacement_str),
                  std::string::npos);
      }
    }
  }
}

}  // namespace updater
