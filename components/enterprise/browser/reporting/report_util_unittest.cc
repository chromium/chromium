// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_util.h"

#include <optional>
#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
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

}  // namespace enterprise_reporting
