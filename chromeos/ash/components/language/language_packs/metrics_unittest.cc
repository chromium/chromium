// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <map>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::language_packs {

TEST(LanguagePackMetricsTest, CheckLanguageCodes) {
  const std::vector<std::string> language_codes = {
      "am", "ar", "be", "bg", "bn", "ca", "cs", "da", "de", "el", "es",
      "et", "fa", "fi", "fr", "ga", "gu", "hi", "hr", "hu", "hy", "id",
      "is", "it", "iw", "ja", "ka", "kk", "km", "kn", "ko", "lo", "lt",
      "lv", "ml", "mn", "mr", "ms", "mt", "my", "ne", "nl", "no", "or",
      "pa", "pl", "pt", "ro", "ru", "si", "sk", "sl", "sr", "sv", "ta",
      "te", "th", "ti", "tl", "tr", "uk", "ur", "vi", "zh"};

  absl::optional<base::HistogramEnumEntryMap> language_codes_map =
      base::ReadEnumFromEnumsXml("LanguagePackLanguageCodes");
  ASSERT_TRUE(language_codes_map)
      << "Error reading enum 'LanguagePackLanguageCodes' from "
         "tools/metrics/histograms/enums.xml.";

  // We prepare the already formatted output in case any code is missing.
  std::string missing_codes;
  for (const std::string& code : language_codes) {
    const auto hashed = static_cast<int32_t>(base::PersistentHash(code));
    if (!base::Contains(*language_codes_map, hashed)) {
      base::StrAppend(&missing_codes,
                      {"<int value=\"", base::NumberToString(hashed),
                       "\" label=\"", code, "\"/>\n"});
    }
  }

  EXPECT_TRUE(missing_codes.empty())
      << "tools/metrics/histograms/enums.xml enum LanguagePackLanguagesCode "
         "doesn't contain all expected language codes. "
      << "Consider adding the following entries:\n\n"
      << missing_codes;
}

}  // namespace ash::language_packs
