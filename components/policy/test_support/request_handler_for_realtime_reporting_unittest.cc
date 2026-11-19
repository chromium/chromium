// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_realtime_reporting.h"

#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/report_storage.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kDeviceToken[] = "test_token";
constexpr char kEventId[] = "test_event_1";

}  // namespace

class RequestHandlerForRealtimeReportingTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForRealtimeReportingTest() = default;
  ~RequestHandlerForRealtimeReportingTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();
    SetURL(test_server()->GetRealtimeReportingURL());
  }
};

TEST_F(RequestHandlerForRealtimeReportingTest, UploadEventsRequestSucceeds) {
  ::chrome::cros::reporting::proto::UploadEventsRequest request;
  request.mutable_device()->set_os_platform("Linux");
  request.mutable_browser()->set_chrome_version("130.0.0.0");
  request.mutable_profile()->set_profile_path("/tmp/profile");
  ::chrome::cros::reporting::proto::Event* event = request.add_events();
  event->set_event_id(kEventId);

  SetDeviceTokenHeader(kDeviceToken);
  SetPayload(request.SerializeAsString());
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  ::chrome::cros::reporting::proto::UploadEventsResponse response;
  ASSERT_TRUE(response.ParseFromString(GetResponseBody()));
  EXPECT_EQ(response.uploaded_event_ids_size(), 1);
  EXPECT_EQ(response.uploaded_event_ids(0), kEventId);
  EXPECT_EQ(report_storage()->GetRealtimeEvents().size(), 1u);

  auto reports = report_storage()->GetRealtimeReports();
  ASSERT_EQ(reports.size(), 1u);
  EXPECT_EQ(reports[0].device().os_platform(), "Linux");
  EXPECT_EQ(reports[0].browser().chrome_version(), "130.0.0.0");
  EXPECT_EQ(reports[0].profile().profile_path(), "/tmp/profile");
  ASSERT_EQ(reports[0].events_size(), 1);
  EXPECT_EQ(reports[0].events(0).event_id(), kEventId);
}

TEST_F(RequestHandlerForRealtimeReportingTest,
       MissingDeviceTokenReturnsUnauthorized) {
  ::chrome::cros::reporting::proto::UploadEventsRequest request;
  ::chrome::cros::reporting::proto::Event* event = request.add_events();
  event->set_event_id(kEventId);

  SetPayload(request.SerializeAsString());
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_UNAUTHORIZED);
}

TEST_F(RequestHandlerForRealtimeReportingTest,
       InvalidPayloadReturnsBadRequest) {
  SetDeviceTokenHeader(kDeviceToken);
  SetPayload("invalid proto");
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

}  // namespace policy
