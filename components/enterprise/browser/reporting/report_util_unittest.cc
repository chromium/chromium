// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_util.h"

#include <optional>
#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace em = enterprise_management;

TEST(ReportUtilTest, GetSecuritySignalsInReportWithCertificates) {
  em::ChromeProfileReportRequest request;
  auto* browser_report = request.mutable_browser_report();
  auto* profile_info = browser_report->add_chrome_user_profile_infos();
  profile_info->mutable_profile_signals_report();

  // Add fake certificates
  auto* cert1 = profile_info->add_certificates();
  cert1->set_data("cert_data_1");
  cert1->set_signature("signature_1");

  auto* cert2 = profile_info->add_certificates();
  cert2->set_data("cert_data_2");
  cert2->set_signature("signature_2");

  profile_info->set_certificates_were_truncated(true);

  std::string json_string = GetSecuritySignalsInReport(request);
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json_string, /*options=*/0);
  ASSERT_TRUE(dict.has_value());

  EXPECT_EQ(dict->FindInt("certificates_count"), 2);
  EXPECT_EQ(dict->FindBool("certificates_were_truncated"), true);
}

TEST(ReportUtilTest, GetSecuritySignalsInReportWithoutCertificates) {
  em::ChromeProfileReportRequest request;
  auto* browser_report = request.mutable_browser_report();
  auto* profile_info = browser_report->add_chrome_user_profile_infos();
  profile_info->mutable_profile_signals_report();

  std::string json_string = GetSecuritySignalsInReport(request);
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json_string, /*options=*/0);
  ASSERT_TRUE(dict.has_value());

  EXPECT_EQ(dict->FindInt("certificates_count"), 0);
  EXPECT_EQ(dict->FindBool("certificates_were_truncated"), false);
}

TEST(ReportUtilTest, GetSecuritySignalsDictInReportWithPolicySummary) {
  em::ChromeProfileReportRequest request;
  auto* profile_info =
      request.mutable_browser_report()->add_chrome_user_profile_infos();
  profile_info->mutable_profile_signals_report();
  profile_info->add_chrome_policies()->set_name("Policy1");
  profile_info->add_chrome_policies()->set_name("Policy2");
  profile_info->add_certificates()->set_data("cert_data");

  base::DictValue dict =
      GetSecuritySignalsDictInReport(request, /*include_policy_summary=*/true);

  // Policies are summarized rather than listed.
  const std::string* chrome_policies = dict.FindString("chrome_policies");
  ASSERT_TRUE(chrome_policies);
  EXPECT_EQ(*chrome_policies,
            "[2 policies collected but omitted for readability]");
  EXPECT_EQ(dict.DebugString().find("Policy1"), std::string::npos);

  // All the fields from `GetSecuritySignalsInReport()` are also included.
  std::optional<base::DictValue> signals_dict = base::JSONReader::ReadDict(
      GetSecuritySignalsInReport(request), /*options=*/0);
  ASSERT_TRUE(signals_dict.has_value());
  for (const auto [key, value] : *signals_dict) {
    const base::Value* debug_value = dict.Find(key);
    ASSERT_TRUE(debug_value) << key;
    EXPECT_EQ(*debug_value, value) << key;
  }
  EXPECT_EQ(dict.FindInt("certificates_count"), 1);
}

TEST(ReportUtilTest,
     GetSecuritySignalsDictInReportWithPolicySummarySinglePolicy) {
  em::ChromeProfileReportRequest request;
  auto* profile_info =
      request.mutable_browser_report()->add_chrome_user_profile_infos();
  profile_info->mutable_profile_signals_report();
  profile_info->add_chrome_policies()->set_name("Policy1");

  base::DictValue dict =
      GetSecuritySignalsDictInReport(request, /*include_policy_summary=*/true);

  const std::string* chrome_policies = dict.FindString("chrome_policies");
  ASSERT_TRUE(chrome_policies);
  EXPECT_EQ(*chrome_policies,
            "[1 policy collected but omitted for readability]");
}

TEST(ReportUtilTest,
     GetSecuritySignalsDictInReportWithPolicySummaryNoPolicies) {
  em::ChromeProfileReportRequest request;
  auto* profile_info =
      request.mutable_browser_report()->add_chrome_user_profile_infos();
  profile_info->mutable_profile_signals_report();

  base::DictValue dict =
      GetSecuritySignalsDictInReport(request, /*include_policy_summary=*/true);
  EXPECT_FALSE(dict.Find("chrome_policies"));
  EXPECT_EQ(dict, GetSecuritySignalsDictInReport(request));
}

TEST(ReportUtilTest, GetSecuritySignalsDictInReportWithoutPolicySummary) {
  em::ChromeProfileReportRequest request;
  auto* profile_info =
      request.mutable_browser_report()->add_chrome_user_profile_infos();
  profile_info->mutable_profile_signals_report();
  profile_info->add_chrome_policies()->set_name("Policy1");

  base::DictValue dict = GetSecuritySignalsDictInReport(request);
  EXPECT_FALSE(dict.Find("chrome_policies"));

  // Same fields as `GetSecuritySignalsInReport()`.
  std::optional<base::DictValue> signals_dict = base::JSONReader::ReadDict(
      GetSecuritySignalsInReport(request), /*options=*/0);
  ASSERT_TRUE(signals_dict.has_value());
  EXPECT_EQ(dict, *signals_dict);
}

// The default output is used for policy logging and the attestation content
// binding, so it must not include the policy summary and must keep its format.
TEST(ReportUtilTest, GetSecuritySignalsInReportDefaultOutputIsUnchanged) {
  em::ChromeProfileReportRequest request;
  auto* profile_info =
      request.mutable_browser_report()->add_chrome_user_profile_infos();
  profile_info->mutable_profile_signals_report();
  profile_info->add_chrome_policies()->set_name("Policy1");

  std::string json_string = GetSecuritySignalsInReport(request);
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json_string, /*options=*/0);
  ASSERT_TRUE(dict.has_value());

  EXPECT_FALSE(dict->Find("chrome_policies"));
  EXPECT_EQ(json_string.find("Policy1"), std::string::npos);

  // Complete reports are pretty-printed.
  std::string expected_json;
  base::JSONWriter::WriteWithOptions(
      *dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &expected_json);
  EXPECT_EQ(json_string, expected_json);
}

TEST(ReportUtilTest,
     GetSecuritySignalsInReportDefaultOutputIncompleteReportIsUnchanged) {
  // A report with more than one profile is incomplete.
  em::ChromeProfileReportRequest request;
  auto* browser_report = request.mutable_browser_report();
  browser_report->add_chrome_user_profile_infos()
      ->add_chrome_policies()
      ->set_name("Policy1");
  browser_report->add_chrome_user_profile_infos()
      ->add_chrome_policies()
      ->set_name("Policy2");

  std::string json_string = GetSecuritySignalsInReport(request);
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json_string, /*options=*/0);
  ASSERT_TRUE(dict.has_value());

  EXPECT_FALSE(dict->Find("chrome_policies"));

  // Incomplete reports are serialized compactly.
  std::string expected_json;
  base::JSONWriter::WriteWithOptions(*dict, /*options=*/0, &expected_json);
  EXPECT_EQ(json_string, expected_json);
}

#if BUILDFLAG(IS_IOS)
TEST(ReportUtilTest, GetSecuritySignalsInReportWithVendorId) {
  em::ChromeProfileReportRequest request;
  auto* os_report = request.mutable_os_report();
  auto* ios_attributes = os_report->mutable_ios_specific_attributes();
  ios_attributes->set_vendor_id("test_vendor_id");

  std::string json_string = GetSecuritySignalsInReport(request);
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json_string, /*options=*/0);
  ASSERT_TRUE(dict.has_value());

  const std::string* vendor_id = dict->FindString("vendor_id");
  ASSERT_TRUE(vendor_id);
  EXPECT_EQ(*vendor_id, "test_vendor_id");
}

TEST(ReportUtilTest, GetSecuritySignalsInReportWithoutVendorId) {
  em::ChromeProfileReportRequest request;
  request.mutable_os_report();

  std::string json_string = GetSecuritySignalsInReport(request);
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json_string, /*options=*/0);
  ASSERT_TRUE(dict.has_value());

  EXPECT_FALSE(dict->FindString("vendor_id"));
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace enterprise_reporting
