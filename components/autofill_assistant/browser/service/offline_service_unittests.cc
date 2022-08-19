// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill_assistant/browser/service/offline_service.h"

#include <memory>
#include "components/autofill_assistant/browser/service/offline_service.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace {
const char kFakeUrl[] = "https://www.example.com";

class OfflineServiceTest : public testing::Test {
 protected:
  ~OfflineServiceTest() override = default;

  base::MockCallback<ServiceRequestSender::ResponseCallback>
      mock_response_callback_;

  SupportsScriptResponseProto supports_site_response_;
  std::string domain_;
  std::vector<
      GetNoRoundtripScriptsByHashPrefixResponseProto::MatchInfo::RoutineScript>
      routines_;

  OfflineService GetService() {
    return OfflineService(
        LocalScriptStore(routines_, domain_, supports_site_response_));
  }
};

TEST_F(OfflineServiceTest, WithRightUrlGetScriptsSucceeds) {
  domain_ = "example.com";
  supports_site_response_.add_scripts()->set_path("test_path");

  EXPECT_CALL(
      mock_response_callback_,
      Run(net::HTTP_OK, supports_site_response_.SerializeAsString(), _));

  GetService().GetScriptsForUrl(GURL(kFakeUrl), TriggerContext(),
                                mock_response_callback_.Get());
}

TEST_F(OfflineServiceTest, WithWrongUrlGetScriptsFails) {
  domain_ = "example.com";
  supports_site_response_.add_scripts()->set_path("test_path");

  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_BAD_REQUEST,
                  std::string(supports_site_response_.SerializeAsString()), _));

  GetService().GetScriptsForUrl(GURL("https://www.wrong.com"), TriggerContext(),
                                mock_response_callback_.Get());
}

TEST_F(OfflineServiceTest, WithExistingPathGetActionsSucceeds) {
  domain_ = "example.com";
  routines_.resize(1);
  routines_[0].mutable_routine()->set_path("script_path");
  routines_[0].mutable_autobot_response()->set_run_id(0);

  EXPECT_CALL(
      mock_response_callback_,
      Run(net::HTTP_OK,
          std::string(routines_.at(0).autobot_response().SerializeAsString()),
          _));

  GetService().GetActions("script_path", GURL(kFakeUrl), TriggerContext(), "",
                          "", mock_response_callback_.Get());
}

TEST_F(OfflineServiceTest, GetActionsFailsGivenWrongPath) {
  domain_ = "example.com";
  routines_.resize(1);
  routines_[0].mutable_routine()->set_path("script_path");
  routines_[0].mutable_autobot_response()->set_run_id(0);

  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_BAD_REQUEST, "", _));

  GetService().GetActions("non_existing_path", GURL(kFakeUrl), TriggerContext(),
                          "", "", mock_response_callback_.Get());
}

TEST_F(OfflineServiceTest, GetNextActionsReturnsEmptyResponse) {
  EXPECT_CALL(mock_response_callback_, Run(net::HTTP_OK, "", _));

  const std::vector<ProcessedActionProto> processed_actions;

  GetService().GetNextActions(TriggerContext(), "", "", processed_actions,
                              RoundtripTimingStats(), RoundtripNetworkStats(),
                              mock_response_callback_.Get());
}

TEST_F(OfflineServiceTest, GetUserDataFails) {
  EXPECT_CALL(mock_response_callback_,
              Run(net::HTTP_METHOD_NOT_ALLOWED, "", _));
  const UserData user_data;
  GetService().GetUserData(CollectUserDataOptions(), 0, &user_data,
                           mock_response_callback_.Get());
}

}  // namespace

}  // namespace autofill_assistant
