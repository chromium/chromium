// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/protocol_translator.h"

#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/feed_response.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/ios_shared_experiments_translator.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/feed_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

const char kResponsePbPath[] = "components/test/data/feed/response.binarypb";
const base::Time kCurrentTime = base::Time::UnixEpoch() + base::Days(123);
AccountInfo TestAccountInfo() {
  AccountInfo account_info;
  account_info.gaia = "gaia";
  account_info.email = "user@foo.com";
  return account_info;
}

feedwire::Response TestWireResponse() {
  // Read and parse response.binarypb.
  base::FilePath response_file_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                               &response_file_path));
  response_file_path = response_file_path.AppendASCII(kResponsePbPath);

  CHECK(base::PathExists(response_file_path));

  std::string response_data;
  CHECK(base::ReadFileToString(response_file_path, &response_data));

  feedwire::Response response;
  CHECK(response.ParseFromString(response_data));
  return response;
}

feedwire::Response EmptyWireResponse() {
  feedwire::Response response;
  response.set_response_version(feedwire::Response::FEED_RESPONSE);
  return response;
}

feedwire::DataOperation MakeDataOperation(
    feedwire::DataOperation::Operation operation) {
  feedwire::DataOperation result;
  result.set_operation(operation);
  result.mutable_metadata()->mutable_content_id()->set_id(42);
  return result;
}

feedwire::DataOperation MakeDataOperationWithContent(
    feedwire::DataOperation::Operation operation,
    std::string xsurface_content = "content") {
  feedwire::DataOperation result = MakeDataOperation(operation);
  result.mutable_feature()->set_renderable_unit(feedwire::Feature::CONTENT);
  result.mutable_feature()
      ->mutable_content()
      ->mutable_xsurface_content()
      ->set_xsurface_output(xsurface_content);

  result.mutable_feature()->mutable_content()->add_prefetch_metadata()->set_uri(
      "http://uri-for-" + xsurface_content);
  return result;
}

feedwire::DataOperation MakeDataOperationWithRenderData(
    feedwire::DataOperation::Operation operation,
    std::string xsurface_render_data = "renderdata") {
  feedwire::DataOperation result = MakeDataOperation(operation);
  result.mutable_render_data()->set_render_data_type(
      feedwire::RenderData::XSURFACE);
  result.mutable_render_data()->mutable_xsurface_container()->set_render_data(
      std::move(xsurface_render_data));
  return result;
}

// Helpers to add some common params.
RefreshResponseData TranslateWireResponse(feedwire::Response response,
                                          const AccountInfo& account_info) {
  return TranslateWireResponse(response,
                               StreamModelUpdateRequest::Source::kNetworkUpdate,
                               account_info, kCurrentTime);
}

RefreshResponseData TranslateWireResponse(feedwire::Response response) {
  return TranslateWireResponse(response, TestAccountInfo());
}

std::optional<feedstore::DataOperation> TranslateDataOperation(
    feedwire::DataOperation operation) {
  return ::feed::TranslateDataOperation(base::Time(), std::move(operation));
}

}  // namespace

class ProtocolTranslatorTest : public testing::Test {
 public:
  ProtocolTranslatorTest() = default;
  ProtocolTranslatorTest(ProtocolTranslatorTest&) = delete;
  ProtocolTranslatorTest& operator=(const ProtocolTranslatorTest&) = delete;
  ~ProtocolTranslatorTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ProtocolTranslatorTest, NextPageToken) {
  feedwire::Response response = EmptyWireResponse();
  feedwire::DataOperation* operation =
      response.mutable_feed_response()->add_data_operation();
  operation->set_operation(feedwire::DataOperation::UPDATE_OR_APPEND);
  operation->mutable_metadata()->mutable_content_id()->set_id(1);
  operation->mutable_next_page_token()
      ->mutable_next_page_token()
      ->set_next_page_token("token");

  RefreshResponseData translated = TranslateWireResponse(response);
  ASSERT_TRUE(translated.model_update_request);
  EXPECT_EQ(1ul, translated.model_update_request->stream_structures.size());
  EXPECT_EQ("token",
            translated.model_update_request->stream_data.next_page_token());
}

TEST_F(ProtocolTranslatorTest, EmptyResponse) {
  feedwire::Response response = EmptyWireResponse();
  EXPECT_TRUE(TranslateWireResponse(response).model_update_request);
}

TEST_F(ProtocolTranslatorTest, RootEventIdPresent) {
  feedwire::Response response = EmptyWireResponse();
  response.mutable_feed_response()
      ->mutable_feed_response_metadata()
      ->mutable_event_id()
      ->set_time_usec(123);
  EXPECT_EQ(TranslateWireResponse(response)
                .model_update_request->stream_data.root_event_id(),
            response.mutable_feed_response()
                ->mutable_feed_response_metadata()
                ->event_id()
                .SerializeAsString());
}

TEST_F(ProtocolTranslatorTest, RootEventIdNotPresent) {
  feedwire::Response response = EmptyWireResponse();
  EXPECT_EQ(TranslateWireResponse(response)
                .model_update_request->stream_data.root_event_id(),
            "");
}

TEST_F(ProtocolTranslatorTest, WasSignedInRequest) {
  feedwire::Response response = EmptyWireResponse();

  for (AccountInfo account_info :
       std::initializer_list<AccountInfo>{{"gaia", "user@foo.com"}, {}}) {
    RefreshResponseData refresh = TranslateWireResponse(response, account_info);
    ASSERT_TRUE(refresh.model_update_request);
    EXPECT_EQ(refresh.model_update_request->stream_data.signed_in(),
              !account_info.IsEmpty());
  }
}

TEST_F(ProtocolTranslatorTest, ActivityLoggingEnabled) {
  feedwire::Response response = EmptyWireResponse();
  for (bool logging_enabled_state : {true, false}) {
    response.mutable_feed_response()
        ->mutable_feed_response_metadata()
        ->mutable_chrome_feed_response_metadata()
        ->set_logging_enabled(logging_enabled_state);
    base::HistogramTester histograms;
    RefreshResponseData refresh = TranslateWireResponse(response);
    ASSERT_TRUE(refresh.model_update_request);
    EXPECT_EQ(refresh.model_update_request->stream_data.logging_enabled(),
              logging_enabled_state);

    // The histogram was updated.
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ActivityLoggingEnabled", logging_enabled_state,
        1);
  }
}

TEST_F(ProtocolTranslatorTest, PrivacyNoticeFulfilled) {
  feedwire::Response response = EmptyWireResponse();
  for (bool privacy_notice_fulfilled_state : {true, false}) {
    response.mutable_feed_response()
        ->mutable_feed_response_metadata()
        ->mutable_chrome_feed_response_metadata()
        ->set_privacy_notice_fulfilled(privacy_notice_fulfilled_state);
    base::HistogramTester histograms;
    RefreshResponseData refresh = TranslateWireResponse(response);
    ASSERT_TRUE(refresh.model_update_request);
    EXPECT_EQ(
        refresh.model_update_request->stream_data.privacy_notice_fulfilled(),
        privacy_notice_fulfilled_state);

    // The histogram was updated.
    histograms.ExpectUniqueSample("ContentSuggestions.Feed.NoticeCardFulfilled",
                                  privacy_notice_fulfilled_state, 1);
  }
}

TEST_F(ProtocolTranslatorTest, ExperimentsAreTranslated) {
  Experiments expected;
  std::vector<ExperimentGroup> group_list{{"Group1", 123}};
  expected["Trial1"] = group_list;

  feedwire::Response response = EmptyWireResponse();
  auto* exp = response.mutable_feed_response()
                  ->mutable_feed_response_metadata()
                  ->mutable_chrome_feed_response_metadata()
                  ->add_experiments();
  exp->set_trial_name("Trial1");
  exp->set_group_name("Group1");
  exp->set_experiment_id("123");

  RefreshResponseData refresh = TranslateWireResponse(response);
  ASSERT_TRUE(refresh.experiments.has_value());

  EXPECT_EQ(refresh.experiments.value(), expected);
}

TEST_F(ProtocolTranslatorTest, ExperimentsAreNotTranslatedGroupAndIDMissing) {
  feedwire::Response response = EmptyWireResponse();
  auto* exp1 = response.mutable_feed_response()
                   ->mutable_feed_response_metadata()
                   ->mutable_chrome_feed_response_metadata()
                   ->add_experiments();
  exp1->set_trial_name("Trial1");

  RefreshResponseData refresh = TranslateWireResponse(response);
  ASSERT_FALSE(refresh.experiments.has_value());
}

TEST_F(ProtocolTranslatorTest, MissingResponseVersion) {
  feedwire::Response response = EmptyWireResponse();
  response.set_response_version(feedwire::Response::UNKNOWN_RESPONSE_VERSION);
  EXPECT_FALSE(TranslateWireResponse(response).model_update_request);
}

TEST_F(ProtocolTranslatorTest, TranslateContent) {
  feedwire::DataOperation wire_operation =
      MakeDataOperationWithContent(feedwire::DataOperation::UPDATE_OR_APPEND);
  std::optional<feedstore::DataOperation> translated =
      TranslateDataOperation(wire_operation);
  EXPECT_TRUE(translated);
  EXPECT_EQ("content", translated->content().frame());
  ASSERT_EQ(1, translated->content().prefetch_metadata_size());
  EXPECT_EQ("http://uri-for-content",
            translated->content().prefetch_metadata(0).uri());
}

TEST_F(ProtocolTranslatorTest, TranslateContentFailsWhenMissingContent) {
  feedwire::DataOperation wire_operation =
      MakeDataOperationWithContent(feedwire::DataOperation::UPDATE_OR_APPEND);
  wire_operation.mutable_feature()->clear_content();
  EXPECT_FALSE(TranslateDataOperation(wire_operation));
}

TEST_F(ProtocolTranslatorTest, TranslateRenderData) {
  feedwire::Response wire_response = EmptyWireResponse();
  *wire_response.mutable_feed_response()->add_data_operation() =
      MakeDataOperationWithRenderData(
          feedwire::DataOperation::UPDATE_OR_APPEND);
  RefreshResponseData translated = TranslateWireResponse(wire_response);
  EXPECT_TRUE(translated.model_update_request);
  ASSERT_EQ(1ul, translated.model_update_request->shared_states.size());
  EXPECT_EQ(
      "renderdata",
      translated.model_update_request->shared_states[0].shared_state_data());
}

TEST_F(ProtocolTranslatorTest, TranslateContentLifetime) {
  feedwire::Response wire_response = EmptyWireResponse();
  feedwire::ContentLifetime* content_lifetime =
      wire_response.mutable_feed_response()
          ->mutable_feed_response_metadata()
          ->mutable_content_lifetime();
  content_lifetime->set_stale_age_ms(123);
  content_lifetime->set_invalid_age_ms(456);
  RefreshResponseData translated = TranslateWireResponse(wire_response);
  ASSERT_TRUE(translated.content_lifetime.has_value());
  EXPECT_EQ(translated.content_lifetime->stale_age_ms(),
            content_lifetime->stale_age_ms());
  EXPECT_EQ(translated.content_lifetime->invalid_age_ms(),
            content_lifetime->invalid_age_ms());
}

TEST_F(ProtocolTranslatorTest, TranslateMissingContentLifetime) {
  feedwire::Response wire_response = EmptyWireResponse();
  RefreshResponseData translated = TranslateWireResponse(wire_response);
  EXPECT_FALSE(translated.content_lifetime.has_value());
}

TEST_F(ProtocolTranslatorTest, TranslateRenderDataFailsWithUnknownType) {
  feedwire::Response wire_response = EmptyWireResponse();
  feedwire::DataOperation wire_operation = MakeDataOperationWithRenderData(
      feedwire::DataOperation::UPDATE_OR_APPEND);
  wire_operation.mutable_render_data()->clear_render_data_type();
  *wire_response.mutable_feed_response()->add_data_operation() =
      std::move(wire_operation);

  RefreshResponseData translated = TranslateWireResponse(wire_response);
  EXPECT_TRUE(translated.model_update_request);
  ASSERT_EQ(0ul, translated.model_update_request->shared_states.size());
}

TEST_F(ProtocolTranslatorTest, RenderDataOperationCanOnlyComeFromFullResponse) {
  EXPECT_FALSE(TranslateDataOperation(MakeDataOperationWithRenderData(
      feedwire::DataOperation::UPDATE_OR_APPEND)));
}

TEST_F(ProtocolTranslatorTest, TranslateOperationFailsWithNoPayload) {
  feedwire::DataOperation wire_operation =
      MakeDataOperationWithContent(feedwire::DataOperation::UPDATE_OR_APPEND);
  wire_operation.clear_feature();
  EXPECT_FALSE(TranslateDataOperation(wire_operation));
}

TEST_F(ProtocolTranslatorTest, TranslateOperationWithoutContentId) {
  feedwire::DataOperation update_operation =
      MakeDataOperationWithContent(feedwire::DataOperation::UPDATE_OR_APPEND);
  update_operation.clear_metadata();
  EXPECT_FALSE(TranslateDataOperation(update_operation));

  feedwire::DataOperation remove_operation =
      MakeDataOperationWithContent(feedwire::DataOperation::REMOVE);
  remove_operation.clear_metadata();
  EXPECT_FALSE(TranslateDataOperation(remove_operation));

  // CLEAR_ALL doesn't need a content ID.
  feedwire::DataOperation clear_operation =
      MakeDataOperation(feedwire::DataOperation::CLEAR_ALL);
  EXPECT_TRUE(TranslateDataOperation(clear_operation));
}

TEST_F(ProtocolTranslatorTest, TranslateOperationFailsWithUnknownOperation) {
  feedwire::DataOperation wire_operation =
      MakeDataOperation(feedwire::DataOperation::UNKNOWN_OPERATION);
  EXPECT_FALSE(TranslateDataOperation(wire_operation));
}

TEST_F(ProtocolTranslatorTest, TranslateRealResponse) {
  // Tests how proto translation works on a real response from the server.
  //
  // The response will periodically need to be updated as changes are made to
  // the server. Update testdata/response.textproto and then run
  // tools/generate_test_response_binarypb.sh.

  feedwire::Response response = TestWireResponse();

  RefreshResponseData translated = TranslateWireResponse(response);

  ASSERT_TRUE(translated.model_update_request);
  ASSERT_TRUE(translated.request_schedule);
  EXPECT_EQ(kCurrentTime, translated.request_schedule->anchor_time);
  EXPECT_EQ(std::vector<base::TimeDelta>(
                {base::Seconds(86308) + base::Nanoseconds(822963644),
                 base::Seconds(120000)}),
            translated.request_schedule->refresh_offsets);

  std::stringstream ss;
  ss << *translated.model_update_request;

  const std::string want = R"(source: 0
stream_data: {
  root_event_id: )"
                           "\"\\b\xEF\xBF\xBD\xEF\xBF\xBD\\u0007\""
                           R"(
  last_added_time_millis: 10627200000
  shared_state_ids {
    content_domain: "render_data"
  }
  content_hashes {
    hashes: 934967784
  }
  content_hashes {
    hashes: 1272916258
  }
  content_hashes {
    hashes: 3242987079
  }
  content_hashes {
    hashes: 1955343871
  }
  content_hashes {
    hashes: 3258315382
  }
  content_hashes {
    hashes: 3546053313
  }
  content_hashes {
    hashes: 1640265464
  }
  content_hashes {
    hashes: 2920920940
  }
  content_hashes {
    hashes: 3805198647
  }
  content_hashes {
    hashes: 3846950793
  }
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 3328940074512586021
  }
  frame: "data2"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 8191455549164721606
  }
  frame: "data3"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 10337142060535577025
  }
  frame: "data4"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 9467333465122011616
  }
  frame: "data5"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 10024917518268143371
  }
  frame: "data6"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 14956621708214864803
  }
  frame: "data7"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 2741853109953412745
  }
  frame: "data8"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 586433679892097787
  }
  frame: "data9"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 790985792726953756
  }
  frame: "data10"
}
content: {
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 7324025093440047528
  }
  frame: "data11"
}
shared_state: {
  content_id {
    content_domain: "render_data"
  }
  shared_state_data: "data1"
}
stream_structure: {
  operation: 1
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "root"
  }
  type: 1
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "render_data"
  }
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 3328940074512586021
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 14679492703605464401
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 14679492703605464401
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 8191455549164721606
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 16663153735812675251
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 16663153735812675251
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 10337142060535577025
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 15532023010474785878
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 15532023010474785878
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 9467333465122011616
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 10111267591181086437
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 10111267591181086437
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 10024917518268143371
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 6703713839373923610
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 6703713839373923610
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 14956621708214864803
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 12592500096310265284
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 12592500096310265284
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 2741853109953412745
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 1016582787945881825
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 1016582787945881825
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 586433679892097787
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 9506447424580769257
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 9506447424580769257
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 790985792726953756
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 17612738377810195843
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 17612738377810195843
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "stories.f"
    type: 1
    id: 7324025093440047528
  }
  parent_id {
    content_domain: "content.f"
    type: 3
    id: 5093490247022575399
  }
  type: 3
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "content.f"
    type: 3
    id: 5093490247022575399
  }
  parent_id {
    content_domain: "root"
  }
  type: 4
}
stream_structure: {
  operation: 2
  content_id {
    content_domain: "request_schedule"
    id: 300842786
  }
}
max_structure_sequence_number: 0
)";
  EXPECT_EQ(want, ss.str());
}

TEST_F(ProtocolTranslatorTest, TranslateDismissData) {
  feedpacking::DismissData input;
  *input.add_data_operations() =
      MakeDataOperation(feedwire::DataOperation::CLEAR_ALL);
  *input.add_data_operations() =
      MakeDataOperationWithContent(feedwire::DataOperation::UPDATE_OR_APPEND);
  std::vector<feedstore::DataOperation> result =
      TranslateDismissData(kCurrentTime, input);

  ASSERT_EQ(2UL, result.size());
  EXPECT_EQ(R"({
  structure {
    operation: 1
  }
}
)",
            ToTextProto(result[0]));
  EXPECT_EQ(R"({
  structure {
    operation: 2
    content_id {
      id: 42
    }
    type: 3
  }
  content {
    content_id {
      id: 42
    }
    frame: "content"
  }
}
)",
            ToTextProto(result[1]));
}

}  // namespace feed
