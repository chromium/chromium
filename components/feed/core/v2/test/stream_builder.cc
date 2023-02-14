// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/test/stream_builder.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/eventid.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"

namespace feed {
namespace {
void AddContentHashes(const feedstore::Content& content,
                      feedstore::StreamData& stream_data) {
  for (auto& metadata : content.prefetch_metadata()) {
    stream_data.add_content_hashes()->add_hashes(
        feedstore::ContentHashFromPrefetchMetadata(metadata));
  }
}
}  // namespace

base::Time kTestTimeEpoch = base::Time::UnixEpoch();
AccountInfo TestAccountInfo() {
  return {"gaia", "user@foo"};
}

ContentId MakeContentId(ContentId::Type type,
                        std::string content_domain,
                        int id_number) {
  ContentId id;
  id.set_content_domain(std::move(content_domain));
  id.set_type(type);
  id.set_id(id_number);
  return id;
}

ContentId MakeClusterId(int id_number) {
  return MakeContentId(ContentId::CLUSTER, "content", id_number);
}

ContentId MakeContentContentId(int id_number) {
  return MakeContentId(ContentId::FEATURE, "stories", id_number);
}

ContentId MakeNoticeCardContentContentId(int id_number) {
  return MakeContentId(ContentId::FEATURE, "privacynoticecard.f", id_number);
}

ContentId MakeSharedStateContentId(int id_number) {
  return MakeContentId(ContentId::TYPE_UNDEFINED, "shared", id_number);
}

ContentId MakeRootId(int id_number) {
  return MakeContentId(ContentId::TYPE_UNDEFINED, "root", id_number);
}

std::string MakeRootEventId(int id_number) {
  feedwire::EventIdMessage id;
  id.set_time_usec(id_number);
  return id.SerializeAsString();
}

ContentId MakeSharedStateId(int id_number) {
  return MakeContentId(ContentId::TYPE_UNDEFINED, "render_data", id_number);
}

feedstore::StreamStructure MakeStream(int id_number) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::STREAM);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  result.set_is_root(true);
  *result.mutable_content_id() = MakeRootId(id_number);
  return result;
}

feedstore::StreamStructure MakeCluster(int id_number, ContentId parent) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::GROUP);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  *result.mutable_content_id() = MakeClusterId(id_number);
  *result.mutable_parent_id() = parent;
  return result;
}

feedstore::StreamStructure MakeNoticeCardCluster(int id_number,
                                                 ContentId parent) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::GROUP);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  *result.mutable_content_id() = MakeClusterId(id_number);
  *result.mutable_parent_id() = parent;
  return result;
}

feedstore::StreamStructure MakeContentNode(int id_number, ContentId parent) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::CONTENT);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  *result.mutable_content_id() = MakeContentContentId(id_number);
  *result.mutable_parent_id() = parent;
  return result;
}

feedstore::StreamStructure MakeNoticeCardContentNode(int id_number,
                                                     ContentId parent) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::CONTENT);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  *result.mutable_content_id() = MakeNoticeCardContentContentId(id_number);
  *result.mutable_parent_id() = parent;
  return result;
}

feedstore::StreamStructure MakeRemove(ContentId id) {
  feedstore::StreamStructure result;
  result.set_operation(feedstore::StreamStructure::REMOVE);
  *result.mutable_content_id() = id;
  return result;
}

feedstore::StreamStructure MakeClearAll() {
  feedstore::StreamStructure result;
  result.set_operation(feedstore::StreamStructure::CLEAR_ALL);
  return result;
}

feedstore::StreamSharedState MakeSharedState(int id_number) {
  feedstore::StreamSharedState shared_state;
  *shared_state.mutable_content_id() = MakeSharedStateId(id_number);
  shared_state.set_shared_state_data("ss:" + base::NumberToString(id_number));
  return shared_state;
}

feedstore::Content MakeContent(int id_number) {
  feedstore::Content result;
  *result.mutable_content_id() = MakeContentContentId(id_number);
  result.set_frame("f:" + base::NumberToString(id_number));
  feedwire::PrefetchMetadata& prefetch_metadata =
      *result.add_prefetch_metadata();

  std::string suffix = base::NumberToString(id_number);
  prefetch_metadata.set_uri("http://content" + suffix);
  prefetch_metadata.set_title("title" + suffix);
  prefetch_metadata.set_publisher("publisher" + suffix);
  prefetch_metadata.set_snippet("snippet" + suffix);
  prefetch_metadata.set_image_url("http://image" + suffix);
  prefetch_metadata.set_favicon_url("http://favicon" + suffix);
  prefetch_metadata.set_badge_id("app/badge" + suffix);
  return result;
}

feedstore::Content MakeNoticeCardContent(int id_number) {
  feedstore::Content result;
  *result.mutable_content_id() = MakeNoticeCardContentContentId(id_number);
  result.set_frame("f:" + base::NumberToString(0));
  return result;
}

feedstore::DataOperation MakeOperation(feedstore::StreamStructure structure) {
  feedstore::DataOperation operation;
  *operation.mutable_structure() = std::move(structure);
  return operation;
}

feedstore::DataOperation MakeOperation(feedstore::Content content) {
  feedstore::DataOperation operation;
  *operation.mutable_content() = std::move(content);
  return operation;
}

feedstore::Record MakeRecord(feedstore::Content content) {
  feedstore::Record record;
  *record.mutable_content() = std::move(content);
  return record;
}

feedstore::Record MakeRecord(
    feedstore::StreamStructureSet stream_structure_set) {
  feedstore::Record record;
  *record.mutable_stream_structures() = std::move(stream_structure_set);
  return record;
}

feedstore::Record MakeRecord(feedstore::StreamSharedState shared_state) {
  feedstore::Record record;
  *record.mutable_shared_state() = std::move(shared_state);
  return record;
}

feedstore::Record MakeRecord(feedstore::StreamData stream_data) {
  feedstore::Record record;
  *record.mutable_stream_data() = std::move(stream_data);
  return record;
}

std::vector<feedstore::DataOperation> MakeTypicalStreamOperations() {
  return {
      MakeOperation(MakeStream()),
      MakeOperation(MakeCluster(0, MakeRootId())),
      MakeOperation(MakeContentNode(0, MakeClusterId(0))),
      MakeOperation(MakeContent(0)),
      MakeOperation(MakeCluster(1, MakeRootId())),
      MakeOperation(MakeContentNode(1, MakeClusterId(1))),
      MakeOperation(MakeContent(1)),
  };
}

StreamModelUpdateRequestGenerator::StreamModelUpdateRequestGenerator() =
    default;
StreamModelUpdateRequestGenerator::~StreamModelUpdateRequestGenerator() =
    default;

std::unique_ptr<StreamModelUpdateRequest>
StreamModelUpdateRequestGenerator::MakeFirstPage(int first_cluster_id,
                                                 int num_cards) const {
  std::vector<int> id_numbers;
  for (int i = first_cluster_id; i < first_cluster_id + num_cards; ++i) {
    id_numbers.push_back(i);
  }
  return MakeFirstPageWithSpecificContents(id_numbers);
}

std::unique_ptr<StreamModelUpdateRequest>
StreamModelUpdateRequestGenerator::MakeFirstPageWithSpecificContents(
    const std::vector<int>& id_numbers) const {
  int first_cluster_id = id_numbers.front();
  bool include_notice_card =
      (privacy_notice_fulfilled && first_cluster_id == 0);

  auto initial_update = std::make_unique<StreamModelUpdateRequest>();
  initial_update->source =
      StreamModelUpdateRequest::Source::kInitialLoadFromStore;
  initial_update->stream_structures = {MakeClearAll(), MakeStream()};

  for (const auto i : id_numbers) {
    if (include_notice_card && i == first_cluster_id) {
      initial_update->content.push_back(MakeNoticeCardContent(i));
      initial_update->stream_structures.push_back(
          MakeNoticeCardCluster(i, MakeRootId()));
      initial_update->stream_structures.push_back(
          MakeNoticeCardContentNode(i, MakeClusterId(i)));
    } else {
      initial_update->content.push_back(MakeContent(i));
      initial_update->stream_structures.push_back(MakeCluster(i, MakeRootId()));
      initial_update->stream_structures.push_back(
          MakeContentNode(i, MakeClusterId(i)));
    }
  }

  initial_update->shared_states.push_back(MakeSharedState(first_cluster_id));
  *initial_update->stream_data.mutable_content_id() = MakeRootId();
  initial_update->stream_data.set_root_event_id(
      MakeRootEventId(event_id_number));
  *initial_update->stream_data.add_shared_state_ids() =
      MakeSharedStateId(first_cluster_id);
  initial_update->stream_data.set_next_page_token("page-2");
  initial_update->stream_data.set_signed_in(signed_in);
  if (signed_in) {
    initial_update->stream_data.set_email(account_info.email);
    initial_update->stream_data.set_gaia(account_info.gaia);
  }
  initial_update->stream_data.set_logging_enabled(logging_enabled);
  initial_update->stream_data.set_privacy_notice_fulfilled(
      privacy_notice_fulfilled);
  initial_update->stream_data.set_stream_key(stream_key);

  for (size_t i = 0; i < id_numbers.size(); ++i) {
    AddContentHashes(initial_update->content[i], initial_update->stream_data);
  }
  feedstore::SetLastAddedTime(last_added_time, initial_update->stream_data);

  return initial_update;
}

std::unique_ptr<StreamModelUpdateRequest>
StreamModelUpdateRequestGenerator::MakeNextPage(
    int page_number,
    StreamModelUpdateRequest::Source source) const {
  auto initial_update = std::make_unique<StreamModelUpdateRequest>();
  initial_update->source = source;
  // Each page has two pieces of content, get their indices.
  const int i = 2 * page_number - 2;
  const int j = i + 1;
  initial_update->content.push_back(MakeContent(i));
  initial_update->content.push_back(MakeContent(j));
  initial_update->stream_structures = {
      MakeStream(), MakeCluster(i, MakeRootId()),
      MakeContentNode(i, MakeClusterId(i)), MakeCluster(j, MakeRootId()),
      MakeContentNode(j, MakeClusterId(j))};

  initial_update->shared_states.push_back(MakeSharedState(page_number));
  *initial_update->stream_data.mutable_content_id() = MakeRootId();
  // This is a different event ID than the first page.
  initial_update->stream_data.set_root_event_id(
      MakeRootEventId(1000 + page_number));
  *initial_update->stream_data.add_shared_state_ids() =
      MakeSharedStateId(page_number);
  initial_update->stream_data.set_next_page_token(
      "page-" + base::NumberToString(page_number + 1));
  initial_update->stream_data.set_signed_in(signed_in);
  if (signed_in) {
    initial_update->stream_data.set_email(account_info.email);
    initial_update->stream_data.set_gaia(account_info.gaia);
  }
  initial_update->stream_data.set_logging_enabled(logging_enabled);
  initial_update->stream_data.set_privacy_notice_fulfilled(
      privacy_notice_fulfilled);

  AddContentHashes(MakeContent(i), initial_update->stream_data);
  AddContentHashes(MakeContent(j), initial_update->stream_data);

  feedstore::SetLastAddedTime(last_added_time, initial_update->stream_data);

  return initial_update;
}

std::unique_ptr<StreamModelUpdateRequest> MakeTypicalInitialModelState(
    int first_cluster_id,
    base::Time last_added_time,
    bool signed_in,
    bool logging_enabled,
    bool privacy_notice_fulfilled,
    std::string stream_key) {
  StreamModelUpdateRequestGenerator generator;
  generator.last_added_time = last_added_time;
  generator.signed_in = signed_in;
  generator.logging_enabled = logging_enabled;
  generator.privacy_notice_fulfilled = privacy_notice_fulfilled;
  generator.stream_key = stream_key;

  return generator.MakeFirstPage(first_cluster_id);
}

std::unique_ptr<StreamModelUpdateRequest> MakeTypicalRefreshModelState(
    int first_cluster_id,
    base::Time last_added_time,
    bool signed_in,
    bool logging_enabled) {
  StreamModelUpdateRequestGenerator generator;
  generator.last_added_time = last_added_time;
  generator.signed_in = signed_in;
  generator.logging_enabled = logging_enabled;
  generator.privacy_notice_fulfilled = false;
  generator.event_id_number = 456;  // Refreshes will have a new event id.
  return generator.MakeFirstPage(first_cluster_id, /*num_cards=*/3);
}

std::unique_ptr<StreamModelUpdateRequest> MakeTypicalNextPageState(
    int page_number,
    base::Time last_added_time,
    bool signed_in,
    bool logging_enabled,
    bool privacy_notice_fulfilled,
    StreamModelUpdateRequest::Source source) {
  StreamModelUpdateRequestGenerator generator;
  generator.last_added_time = last_added_time;
  generator.signed_in = signed_in;
  generator.logging_enabled = logging_enabled;
  generator.privacy_notice_fulfilled = privacy_notice_fulfilled;
  return generator.MakeNextPage(page_number, source);
}

feedstore::WebFeedInfo MakeWebFeedInfo(const std::string& name) {
  feedstore::WebFeedInfo result;
  result.set_web_feed_id("id_" + name);
  result.set_title("Title " + name);
  result.mutable_favicon()->set_url("http://favicon/" + name);
  result.set_follower_count(123);
  result.set_visit_uri("https://" + name + ".com");
  feedwire::webfeed::WebFeedMatcher* matcher = result.add_matchers();
  feedwire::webfeed::WebFeedMatcher::Criteria* criteria =
      matcher->add_criteria();
  criteria->set_criteria_type(
      feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_HOST_SUFFIX);
  criteria->set_text(name + ".com");
  return result;
}

feedwire::webfeed::WebFeed MakeWireWebFeed(const std::string& name) {
  feedwire::webfeed::WebFeed result;
  result.set_name("id_" + name);
  result.set_title("Title " + name);
  result.set_subtitle("Subtitle " + name);
  result.set_detail_text("details...");
  result.set_visit_uri("https://" + name + ".com");
  result.set_follower_count(kFollowerCount);
  *result.add_web_feed_matchers() = MakeDomainMatcher(name + ".com");
  return result;
}

feedwire::webfeed::FollowWebFeedResponse SuccessfulFollowResponse(
    const std::string& follow_name) {
  feedwire::webfeed::FollowWebFeedResponse response;
  *response.mutable_web_feed() = MakeWireWebFeed(follow_name);
  SetConsistencyToken(response, "follow-ct");
  return response;
}

feedwire::webfeed::UnfollowWebFeedResponse SuccessfulUnfollowResponse() {
  feedwire::webfeed::UnfollowWebFeedResponse response;
  SetConsistencyToken(response, "unfollow-ct");
  return response;
}
feedwire::webfeed::QueryWebFeedResponse SuccessfulQueryResponse(
    const std::string& query_name) {
  feedwire::webfeed::QueryWebFeedResponse response;
  *response.mutable_web_feed() = MakeWireWebFeed(query_name);
  SetConsistencyToken(response, "query-ct");
  return response;
}

WebFeedPageInformation MakeWebFeedPageInformation(const std::string& url) {
  WebFeedPageInformation info;
  info.SetUrl(GURL(url));
  return info;
}

feedwire::webfeed::WebFeedMatcher MakeDomainMatcher(const std::string& domain) {
  feedwire::webfeed::WebFeedMatcher result;
  feedwire::webfeed::WebFeedMatcher::Criteria* criteria = result.add_criteria();
  criteria->set_criteria_type(
      feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_HOST_SUFFIX);
  criteria->set_text(domain);
  return result;
}

}  // namespace feed
