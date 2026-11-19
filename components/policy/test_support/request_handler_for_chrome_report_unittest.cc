// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_chrome_report.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/report_storage.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kBrowserVersion[] = "130.0.6723.58";

}  // namespace

class RequestHandlerForChromeReportTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(RequestHandlerForChromeReportTest, DesktopReportSucceeds) {
  SetRequestTypeParam(dm_protocol::kValueRequestChromeDesktopReport);
  em::DeviceManagementRequest request;
  request.mutable_chrome_desktop_report_request()
      ->mutable_browser_report()
      ->set_browser_version(kBrowserVersion);
  SetPayload(request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  EXPECT_TRUE(
      GetDeviceManagementResponse().has_chrome_desktop_report_response());
  ASSERT_EQ(report_storage()->GetBrowserReports().size(), 1u);
  EXPECT_EQ(report_storage()->GetBrowserReports()[0].device_id, kDeviceId);
  EXPECT_EQ(report_storage()
                ->GetBrowserReports()[0]
                .report.browser_report()
                .browser_version(),
            kBrowserVersion);
}

TEST_F(RequestHandlerForChromeReportTest, ProfileReportSucceeds) {
  SetRequestTypeParam(dm_protocol::kValueRequestChromeProfileReport);
  em::DeviceManagementRequest request;
  request.mutable_chrome_profile_report_request()
      ->mutable_browser_report()
      ->set_browser_version(kBrowserVersion);
  SetPayload(request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  EXPECT_TRUE(
      GetDeviceManagementResponse().has_chrome_profile_report_response());
  ASSERT_EQ(report_storage()->GetProfileReports().size(), 1u);
  EXPECT_EQ(report_storage()->GetProfileReports()[0].device_id, kDeviceId);
  EXPECT_EQ(report_storage()
                ->GetProfileReports()[0]
                .report.browser_report()
                .browser_version(),
            kBrowserVersion);
}

TEST_F(RequestHandlerForChromeReportTest,
       DesktopReportEmptyBodyReturnsBadRequest) {
  SetRequestTypeParam(dm_protocol::kValueRequestChromeDesktopReport);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
  EXPECT_TRUE(report_storage()->GetBrowserReports().empty());
}

TEST_F(RequestHandlerForChromeReportTest,
       DesktopReportWithProfilePayloadReturnsBadRequest) {
  SetRequestTypeParam(dm_protocol::kValueRequestChromeDesktopReport);
  em::DeviceManagementRequest request;
  request.mutable_chrome_profile_report_request();
  SetPayload(request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
  EXPECT_TRUE(report_storage()->GetBrowserReports().empty());
}

TEST_F(RequestHandlerForChromeReportTest,
       ProfileReportEmptyBodyReturnsBadRequest) {
  SetRequestTypeParam(dm_protocol::kValueRequestChromeProfileReport);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
  EXPECT_TRUE(report_storage()->GetProfileReports().empty());
}

TEST_F(RequestHandlerForChromeReportTest,
       ProfileReportWithDesktopPayloadReturnsBadRequest) {
  SetRequestTypeParam(dm_protocol::kValueRequestChromeProfileReport);
  em::DeviceManagementRequest request;
  request.mutable_chrome_desktop_report_request();
  SetPayload(request);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
  EXPECT_TRUE(report_storage()->GetProfileReports().empty());
}

TEST_F(RequestHandlerForChromeReportTest, InvalidProtoReturnsBadRequest) {
  SetRequestTypeParam(dm_protocol::kValueRequestChromeDesktopReport);
  SetPayload("invalid proto");

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
  EXPECT_TRUE(report_storage()->GetBrowserReports().empty());
}

}  // namespace policy
