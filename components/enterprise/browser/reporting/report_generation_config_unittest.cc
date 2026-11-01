// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_generation_config.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

TEST(ReportGenerationConfigTest, ToString_NoSelectors) {
  ReportGenerationConfig config(ReportTrigger::kTriggerTimer);
  EXPECT_EQ(config.ToString(),
            "Trigger: Periodic timer expired\n"
            "Report Type: Browser Report\n"
            "Security Signals Mode: No security signals reported\n"
            "Using Cookies: No\n"
            "Has Challenge: No\n"
            "Is Retrying: No\n"
            "Selectors: []");
}

TEST(ReportGenerationConfigTest, ToString_WithSelectors) {
  base::ListValue selectors;
  selectors.Append(base::DictValue().Set(
      "ISSUER", base::DictValue().Set("CN", "Test Root CA")));
  selectors.Append(base::DictValue().Set(
      "SUBJECT", base::DictValue().Set("O", "Google Test")));
  base::ListValue expected_selectors = selectors.Clone();

  ReportGenerationConfig config(ReportTrigger::kTriggerSecurity,
                                ReportType::kProfileReport,
                                SecuritySignalsMode::kSignalsOnly,
                                /*use_cookies=*/true, "challenge",
                                std::move(selectors), /*is_retrying=*/false);

  std::string result = config.ToString();
  constexpr char kExpectedPrefix[] =
      "Trigger: Trigger for security signals\n"
      "Report Type: Profile Report\n"
      "Security Signals Mode: Security signals are reported exclusively\n"
      "Using Cookies: Yes\n"
      "Has Challenge: Yes\n"
      "Is Retrying: No\n"
      "Selectors: ";
  ASSERT_TRUE(base::StartsWith(result, kExpectedPrefix));

  // The selectors are pretty-printed JSON spanning multiple lines.
  std::string selectors_json = result.substr(sizeof(kExpectedPrefix) - 1);
  EXPECT_NE(selectors_json.find('\n'), std::string::npos);
  EXPECT_FALSE(base::EndsWith(selectors_json, "\n"));
  std::optional<base::ListValue> parsed =
      base::JSONReader::ReadList(selectors_json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed);
  EXPECT_EQ(*parsed, expected_selectors);
}

}  // namespace enterprise_reporting
