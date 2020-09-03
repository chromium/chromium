// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/test/stream_builder.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"

namespace feed {

const base::Time kTestTimeEpoch = base::Time::UnixEpoch();

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

ContentId MakeSharedStateContentId(int id_number) {
  return MakeContentId(ContentId::TYPE_UNDEFINED, "shared", id_number);
}

ContentId MakeRootId(int id_number) {
  return MakeContentId(ContentId::TYPE_UNDEFINED, "root", id_number);
}

ContentId MakeSharedStateId(int id_number) {
  return MakeContentId(ContentId::TYPE_UNDEFINED, "render_data", id_number);
}

feedstore::StreamStructure MakeStream(int id_number) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::STREAM);
  result.set_operation(feedstore::StreamStructure::UPDATE_OR_APPEND);
  *result.mutable_content_id() = MakeRootId(id_number);
  return result;
}

feedstore::StreamStructure MakeCluster(int id_number, ContentId parent) {
  feedstore::StreamStructure result;
  result.set_type(feedstore::StreamStructure::CLUSTER);
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

std::unique_ptr<StreamModelUpdateRequest> MakeTypicalInitialModelState(
    int first_cluster_id,
    base::Time last_added_time,
    bool signed_in,
    bool logging_enabled,
    bool privacy_notice_fulfilled) {
  auto initial_update = std::make_unique<StreamModelUpdateRequest>();
  const int i = first_cluster_id;
  const int j = first_cluster_id + 1;
  initial_update->source =
      StreamModelUpdateRequest::Source::kInitialLoadFromStore;
  initial_update->content.push_back(MakeContent(i));
  initial_update->content.push_back(MakeContent(j));
  initial_update->stream_structures = {MakeClearAll(),
                                       MakeStream(),
                                       MakeCluster(i, MakeRootId()),
                                       MakeContentNode(i, MakeClusterId(i)),
                                       MakeCluster(j, MakeRootId()),
                                       MakeContentNode(j, MakeClusterId(j))};

  initial_update->shared_states.push_back(MakeSharedState(i));
  *initial_update->stream_data.mutable_content_id() = MakeRootId();
  *initial_update->stream_data.mutable_shared_state_id() = MakeSharedStateId(i);
  initial_update->stream_data.set_next_page_token("page-2");
  initial_update->stream_data.set_signed_in(signed_in);
  initial_update->stream_data.set_logging_enabled(logging_enabled);
  initial_update->stream_data.set_privacy_notice_fulfilled(
      privacy_notice_fulfilled);
  SetLastAddedTime(last_added_time, initial_update->stream_data);

  return initial_update;
}

std::unique_ptr<StreamModelUpdateRequest> MakeTypicalNextPageState(
    int page_number,
    base::Time last_added_time,
    bool signed_in,
    bool logging_enabled,
    bool privacy_notice_fulfilled) {
  auto initial_update = std::make_unique<StreamModelUpdateRequest>();
  initial_update->source =
      StreamModelUpdateRequest::Source::kInitialLoadFromStore;
  // Each page has two pieces of content, get their indices.
  const int i = 2 * page_number - 2;
  const int j = i + 1;
  initial_update->content.push_back(MakeContent(i));
  initial_update->content.push_back(MakeContent(j));
  initial_update->stream_structures = {
      MakeStream(), MakeCluster(i, MakeRootId()),
      MakeContentNode(i, MakeClusterId(i)), MakeCluster(j, MakeRootId()),
      MakeContentNode(j, MakeClusterId(j))};

  initial_update->shared_states.push_back(MakeSharedState(0));
  *initial_update->stream_data.mutable_content_id() = MakeRootId();
  *initial_update->stream_data.mutable_shared_state_id() = MakeSharedStateId(0);
  initial_update->stream_data.set_next_page_token(
      "page-" + base::NumberToString(page_number + 1));
  initial_update->stream_data.set_signed_in(signed_in);
  initial_update->stream_data.set_logging_enabled(logging_enabled);
  initial_update->stream_data.set_privacy_notice_fulfilled(
      privacy_notice_fulfilled);
  SetLastAddedTime(last_added_time, initial_update->stream_data);

  return initial_update;
}

}  // namespace feed
