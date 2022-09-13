// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/local_script_store.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/version_info/version_info.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class LocalScriptStoreTest : public testing::Test {
 public:
  std::unique_ptr<LocalScriptStore> GetStore() {
    return std::make_unique<LocalScriptStore>(routines_, domain_,
                                              supports_site_response_);
  }

 protected:
  SupportsScriptResponseProto supports_site_response_;
  std::string domain_;
  std::vector<
      GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo::RoutineScript>
      routines_;
};

TEST_F(LocalScriptStoreTest, IsEmptyWithoutRoutines) {
  domain_ = "test";
  EXPECT_TRUE(GetStore()->empty());
  EXPECT_EQ(GetStore()->size(), 0u);
}

TEST_F(LocalScriptStoreTest, IsEmptyWithoutDomain) {
  routines_.emplace_back();
  EXPECT_TRUE(GetStore()->empty());
  EXPECT_EQ(GetStore()->size(), 0ul);
}

TEST_F(LocalScriptStoreTest, IsNotEmptyWithRoutinesAndDomain) {
  routines_.emplace_back();
  domain_ = "test";
  EXPECT_FALSE(GetStore()->empty());
  EXPECT_EQ(GetStore()->size(), 1ul);
}

TEST_F(LocalScriptStoreTest, GetRoutinesRetrievesRoutines) {
  GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo::RoutineScript
      routine;
  routine.set_script_path("test");
  routines_.push_back(routine);
  ASSERT_EQ(GetStore()->GetRoutines().size(), 1ul);
  EXPECT_EQ(GetStore()->GetRoutines()[0].script_path(), "test");
}

TEST_F(LocalScriptStoreTest, GetDomainRetrievesDomain) {
  domain_ = "test";
  EXPECT_EQ(GetStore()->GetDomain(), domain_);
}

TEST_F(LocalScriptStoreTest, GetSupportsSiteRetrievesSupportsSiteResponse) {
  supports_site_response_.add_scripts()->set_path("test");
  ASSERT_EQ(GetStore()->GetSupportsSiteResponse().scripts_size(), 1);
  EXPECT_EQ(GetStore()->GetSupportsSiteResponse().scripts()[0].path(), "test");
}

TEST_F(LocalScriptStoreTest, GetEmptyRoutinesByDefault) {
  EXPECT_TRUE(GetStore()->GetRoutines().empty());
}

TEST_F(LocalScriptStoreTest, GetEmptyDomainByDefault) {
  EXPECT_EQ(GetStore()->GetDomain(), "");
}

TEST_F(LocalScriptStoreTest, GetEmptySupportsSiteResponseByDefault) {
  ASSERT_EQ(GetStore()->GetSupportsSiteResponse().scripts_size(), 0);
}

}  // namespace autofill_assistant
