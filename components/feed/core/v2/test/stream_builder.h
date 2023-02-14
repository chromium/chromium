// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TEST_STREAM_BUILDER_H_
#define COMPONENTS_FEED_CORE_V2_TEST_STREAM_BUILDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/types.h"

// Functions that help build a feedstore::StreamStructure for testing.
namespace feed {
struct StreamModelUpdateRequest;

extern base::Time kTestTimeEpoch;
constexpr int64_t kFollowerCount = 123;

AccountInfo TestAccountInfo();
ContentId MakeContentId(ContentId::Type type,
                        std::string content_domain,
                        int id_number);
ContentId MakeClusterId(int id_number);
ContentId MakeNoticeCardClusterId();
ContentId MakeContentContentId(int id_number);
ContentId MakeNoticeCardContentContentId(int id_number);
ContentId MakeSharedStateContentId(int id_number);
ContentId MakeRootId(int id_number = 0);
ContentId MakeSharedStateId(int id_number = 0);
std::string MakeRootEventId(int id_number = 123);
feedstore::StreamStructure MakeStream(int id_number = 0);
feedstore::StreamStructure MakeCluster(int id_number, ContentId parent);
feedstore::StreamStructure MakeNoticeCardCluster(ContentId parent);
feedstore::StreamStructure MakeContentNode(int id_number, ContentId parent);
feedstore::StreamStructure MakeNoticeCardContentNode(int id_number,
                                                     ContentId parent);
feedstore::StreamSharedState MakeSharedState(int id_number);
feedstore::StreamStructure MakeRemove(ContentId id);
feedstore::StreamStructure MakeClearAll();
feedstore::Content MakeContent(int id_number);
feedstore::Content MakeNoticeCardContent();
feedstore::DataOperation MakeOperation(feedstore::StreamStructure structure);
feedstore::DataOperation MakeOperation(feedstore::Content content);
feedstore::Record MakeRecord(feedstore::Content content);
feedstore::Record MakeRecord(
    feedstore::StreamStructureSet stream_structure_set);
feedstore::Record MakeRecord(feedstore::StreamSharedState shared_state);
feedstore::Record MakeRecord(feedstore::StreamData stream_data);

// Helper structure to configure and return RefreshResponseData and
// StreamModelUpdateRequest objects denoting typical initial and next page
// refresh response payloads.
struct StreamModelUpdateRequestGenerator {
  base::Time last_added_time = kTestTimeEpoch;
  base::Time last_server_response_time = kTestTimeEpoch;
  bool signed_in = true;
  AccountInfo account_info = TestAccountInfo();
  bool logging_enabled = true;
  bool privacy_notice_fulfilled = false;
  int event_id_number = 123;
  std::string stream_key =
      feedstore::StreamKey(StreamType(StreamKind::kForYou));

  StreamModelUpdateRequestGenerator();
  ~StreamModelUpdateRequestGenerator();

  std::unique_ptr<StreamModelUpdateRequest> MakeFirstPage(
      int first_cluster_id = 0,
      int num_cards = 2) const;
  std::unique_ptr<StreamModelUpdateRequest> MakeFirstPageWithSpecificContents(
      const std::vector<int>& id_numbers) const;

  std::unique_ptr<StreamModelUpdateRequest> MakeNextPage(
      int page_number = 2,
      StreamModelUpdateRequest::Source source =
          StreamModelUpdateRequest::Source::kInitialLoadFromStore) const;
};

// Returns data operations to create a typical stream:
// Root
// |-Cluster 0
// |  |-Content 0
// |-Cluster 1
//    |-Content 1
std::vector<feedstore::DataOperation> MakeTypicalStreamOperations();
std::unique_ptr<StreamModelUpdateRequest> MakeTypicalInitialModelState(
    int first_cluster_id = 0,
    base::Time last_added_time = kTestTimeEpoch,
    bool signed_in = true,
    bool logging_enabled = true,
    bool privacy_notice_fulfilled = false,
    std::string stream_key =
        feedstore::StreamKey(StreamType(StreamKind::kForYou)));
// Returns data operations to create a typical stream for refreshing:
// Root
// |-Cluster 2
// |  |-Content 2
// |-Cluster 3
// |  |-Content 3
// |-Cluster 4
//    |-Content 4
std::unique_ptr<StreamModelUpdateRequest> MakeTypicalRefreshModelState(
    int first_cluster_id = 2,
    base::Time last_added_time = kTestTimeEpoch,
    bool signed_in = true,
    bool logging_enabled = true);
// Root
// |-Cluster 2
// |  |-Content 2
// |-Cluster 3
//    |-Content 3
std::unique_ptr<StreamModelUpdateRequest> MakeTypicalNextPageState(
    int page_number = 2,
    base::Time last_added_time = kTestTimeEpoch,
    bool signed_in = true,
    bool logging_enabled = true,
    bool privacy_notice_fulfilled = true,
    StreamModelUpdateRequest::Source source =
        StreamModelUpdateRequest::Source::kNetworkLoadMore);

feedwire::webfeed::WebFeed MakeWireWebFeed(const std::string& name);
feedstore::WebFeedInfo MakeWebFeedInfo(const std::string& name);
WebFeedPageInformation MakeWebFeedPageInformation(const std::string& url);
feedwire::webfeed::FollowWebFeedResponse SuccessfulFollowResponse(
    const std::string& follow_name);
feedwire::webfeed::UnfollowWebFeedResponse SuccessfulUnfollowResponse();
feedwire::webfeed::QueryWebFeedResponse SuccessfulQueryResponse(
    const std::string& query_name);
feedwire::webfeed::WebFeedMatcher MakeDomainMatcher(const std::string& domain);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TEST_STREAM_BUILDER_H_
