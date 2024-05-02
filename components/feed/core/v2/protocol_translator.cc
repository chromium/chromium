// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/protocol_translator.h"

#include <optional>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/packing.pb.h"
#include "components/feed/core/proto/v2/wire/chrome_feed_response_metadata.pb.h"
#include "components/feed/core/proto/v2/wire/data_operation.pb.h"
#include "components/feed/core/proto/v2/wire/feature.pb.h"
#include "components/feed/core/proto/v2/wire/feed_response.pb.h"
#include "components/feed/core/proto/v2/wire/payload_metadata.pb.h"
#include "components/feed/core/proto/v2/wire/request_schedule.pb.h"
#include "components/feed/core/proto/v2/wire/stream_structure.pb.h"
#include "components/feed/core/proto/v2/wire/token.pb.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/ios_shared_experiments_translator.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/feed_feature_list.h"

namespace feed {

namespace {

feedstore::StreamStructure::Operation TranslateOperationType(
    feedwire::DataOperation::Operation operation) {
  switch (operation) {
    case feedwire::DataOperation::UNKNOWN_OPERATION:
      return feedstore::StreamStructure::UNKNOWN;
    case feedwire::DataOperation::CLEAR_ALL:
      return feedstore::StreamStructure::CLEAR_ALL;
    case feedwire::DataOperation::UPDATE_OR_APPEND:
      return feedstore::StreamStructure::UPDATE_OR_APPEND;
    case feedwire::DataOperation::REMOVE:
      return feedstore::StreamStructure::REMOVE;
    default:
      return feedstore::StreamStructure::UNKNOWN;
  }
}

feedstore::StreamStructure::Type TranslateNodeType(
    feedwire::Feature::RenderableUnit renderable_unit) {
  switch (renderable_unit) {
    case feedwire::Feature::UNKNOWN_RENDERABLE_UNIT:
      return feedstore::StreamStructure::UNKNOWN_TYPE;
    case feedwire::Feature::STREAM:
      return feedstore::StreamStructure::STREAM;
    case feedwire::Feature::CONTENT:
      return feedstore::StreamStructure::CONTENT;
    case feedwire::Feature::GROUP:
      return feedstore::StreamStructure::GROUP;
    default:
      return feedstore::StreamStructure::UNKNOWN_TYPE;
  }
}

feedstore::Metadata::StreamMetadata::ContentLifetime TranslateContentLifetime(
    feedwire::ContentLifetime content_lifetime) {
  feedstore::Metadata::StreamMetadata::ContentLifetime store_lifetime;
  store_lifetime.set_stale_age_ms(content_lifetime.stale_age_ms());
  store_lifetime.set_invalid_age_ms(content_lifetime.invalid_age_ms());

  return store_lifetime;
}

base::TimeDelta TranslateDuration(const feedwire::Duration& v) {
  return base::Seconds(v.seconds()) + base::Nanoseconds(v.nanos());
}

std::optional<RequestSchedule> TranslateRequestSchedule(
    base::Time now,
    const feedwire::RequestSchedule& v) {
  RequestSchedule schedule;
  const feedwire::RequestSchedule_TimeBasedSchedule& time_schedule =
      v.time_based_schedule();
  for (const feedwire::Duration& duration :
       time_schedule.refresh_time_from_response_time()) {
    schedule.refresh_offsets.push_back(TranslateDuration(duration));
  }
  schedule.anchor_time = now;
  return schedule;
}

// Fields that should be present at most once in the response.
struct ConvertedGlobalData {
  std::optional<RequestSchedule> request_schedule;
};

struct ConvertedDataOperation {
  feedstore::StreamStructure stream_structure;
  std::optional<feedstore::Content> content;
  std::optional<feedstore::StreamSharedState> shared_state;
  std::optional<std::string> next_page_token;
};

bool TranslateFeature(feedwire::Feature* feature,
                      ConvertedDataOperation& result) {
  feedstore::StreamStructure::Type type =
      TranslateNodeType(feature->renderable_unit());
  result.stream_structure.set_type(type);

  if (type == feedstore::StreamStructure::CONTENT) {
    feedwire::Content* wire_content = feature->mutable_content();

    if (!wire_content->has_xsurface_content())
      return false;

    // TODO(iwells): We still need score, availability_time_seconds,
    // offline_metadata, and representation_data to populate content_info.

    result.content.emplace();
    *(result.content->mutable_content_id()) =
        result.stream_structure.content_id();
    result.content->set_allocated_frame(
        wire_content->mutable_xsurface_content()->release_xsurface_output());
    if (wire_content->prefetch_metadata_size() > 0) {
      result.content->mutable_prefetch_metadata()->Swap(
          wire_content->mutable_prefetch_metadata());
    }
  }
  return true;
}

std::optional<feedstore::StreamSharedState> TranslateSharedState(
    feedwire::ContentId content_id,
    feedwire::RenderData& wire_shared_state) {
  if (wire_shared_state.render_data_type() != feedwire::RenderData::XSURFACE) {
    return std::nullopt;
  }

  feedstore::StreamSharedState shared_state;
  *shared_state.mutable_content_id() = std::move(content_id);
  shared_state.set_allocated_shared_state_data(
      wire_shared_state.mutable_xsurface_container()->release_render_data());
  return shared_state;
}

bool TranslatePayload(base::Time now,
                      feedwire::DataOperation operation,
                      ConvertedGlobalData* global_data,
                      ConvertedDataOperation& result) {
  switch (operation.payload_case()) {
    case feedwire::DataOperation::kFeature: {
      feedwire::Feature* feature = operation.mutable_feature();
      DCHECK(!result.stream_structure.has_parent_id());
      if (feature->has_parent_id()) {
        result.stream_structure.set_allocated_parent_id(
            feature->release_parent_id());
      } else if (feature->is_root()) {
        result.stream_structure.set_is_root(true);
      }

      if (!TranslateFeature(feature, result))
        return false;
    } break;
    case feedwire::DataOperation::kNextPageToken: {
      feedwire::Token* token = operation.mutable_next_page_token();
      result.stream_structure.set_allocated_parent_id(
          token->release_parent_id());
      result.next_page_token = std::move(
          *token->mutable_next_page_token()->mutable_next_page_token());
    } break;
    case feedwire::DataOperation::kRenderData: {
      result.shared_state =
          TranslateSharedState(result.stream_structure.content_id(),
                               *operation.mutable_render_data());
      if (!result.shared_state)
        return false;
    } break;
    case feedwire::DataOperation::kRequestSchedule: {
      if (global_data) {
        global_data->request_schedule =
            TranslateRequestSchedule(now, operation.request_schedule());
      }
    } break;

    default:
      return false;
  }

  return true;
}

std::optional<ConvertedDataOperation> TranslateDataOperationInternal(
    base::Time now,
    feedwire::DataOperation operation,
    ConvertedGlobalData* global_data) {
  feedstore::StreamStructure::Operation operation_type =
      TranslateOperationType(operation.operation());

  ConvertedDataOperation result;
  result.stream_structure.set_operation(operation_type);

  switch (operation_type) {
    case feedstore::StreamStructure::CLEAR_ALL:
      return result;

    case feedstore::StreamStructure::UPDATE_OR_APPEND:
      if (!operation.has_metadata() || !operation.metadata().has_content_id())
        return std::nullopt;

      result.stream_structure.set_allocated_content_id(
          operation.mutable_metadata()->release_content_id());

      if (!TranslatePayload(now, std::move(operation), global_data, result))
        return std::nullopt;
      break;

    case feedstore::StreamStructure::REMOVE:
      if (!operation.has_metadata() || !operation.metadata().has_content_id())
        return std::nullopt;

      result.stream_structure.set_allocated_content_id(
          operation.mutable_metadata()->release_content_id());
      break;

    case feedstore::StreamStructure::UNKNOWN:  // Fall through
    default:
      return std::nullopt;
  }

  return result;
}

}  // namespace

StreamModelUpdateRequest::StreamModelUpdateRequest() = default;
StreamModelUpdateRequest::~StreamModelUpdateRequest() = default;
StreamModelUpdateRequest::StreamModelUpdateRequest(
    const StreamModelUpdateRequest&) = default;
StreamModelUpdateRequest& StreamModelUpdateRequest::operator=(
    const StreamModelUpdateRequest&) = default;

RefreshResponseData::RefreshResponseData() = default;
RefreshResponseData::~RefreshResponseData() = default;
RefreshResponseData::RefreshResponseData(RefreshResponseData&&) = default;
RefreshResponseData& RefreshResponseData::operator=(RefreshResponseData&&) =
    default;

std::optional<feedstore::DataOperation> TranslateDataOperation(
    base::Time now,
    feedwire::DataOperation wire_operation) {
  feedstore::DataOperation store_operation;
  // Note: This function is used when executing operations in response to
  // actions embedded in the server protobuf. Some data in data operations
  // aren't supported by this function, which is why we're passing in
  // global_data=nullptr.
  std::optional<ConvertedDataOperation> converted =
      TranslateDataOperationInternal(now, std::move(wire_operation), nullptr);
  if (!converted)
    return std::nullopt;

  // We only support translating StreamSharedStates when they will be attached
  // to StreamModelUpdateRequests.
  if (converted->shared_state)
    return std::nullopt;

  *store_operation.mutable_structure() = std::move(converted->stream_structure);
  if (converted->content)
    *store_operation.mutable_content() = std::move(*converted->content);

  return store_operation;
}

RefreshResponseData TranslateWireResponse(
    feedwire::Response response,
    StreamModelUpdateRequest::Source source,
    const AccountInfo& account_info,
    base::Time current_time) {
  if (response.response_version() != feedwire::Response::FEED_RESPONSE)
    return {};

  auto result = std::make_unique<StreamModelUpdateRequest>();
  result->source = source;

  ConvertedGlobalData global_data;
  feedwire::FeedResponse* feed_response = response.mutable_feed_response();
  for (auto& wire_data_operation : *feed_response->mutable_data_operation()) {
    if (!wire_data_operation.has_operation())
      continue;

    std::optional<ConvertedDataOperation> operation =
        TranslateDataOperationInternal(
            current_time, std::move(wire_data_operation), &global_data);
    if (!operation)
      continue;

    result->stream_structures.push_back(std::move(operation->stream_structure));

    if (operation->content)
      result->content.push_back(std::move(*operation->content));

    if (operation->shared_state)
      result->shared_states.push_back(std::move(*operation->shared_state));

    if (operation->next_page_token) {
      result->stream_data.set_next_page_token(
          std::move(*operation->next_page_token));
    }
  }

  if (!result->shared_states.empty()) {
    for (const auto& shared_state : result->shared_states) {
      *result->stream_data.add_shared_state_ids() = shared_state.content_id();
    }
  }
  feedstore::SetLastAddedTime(current_time, result->stream_data);

  const auto& response_metadata = feed_response->feed_response_metadata();

  std::optional<feedstore::Metadata::StreamMetadata::ContentLifetime>
      content_lifetime;
  if (response_metadata.has_content_lifetime()) {
    content_lifetime =
        TranslateContentLifetime(response_metadata.content_lifetime());
  }

  const auto& chrome_response_metadata =
      response_metadata.chrome_feed_response_metadata();
  // Note that we're storing the raw proto bytes for the root event ID because
  // we want to retain any unknown fields. This value is ignored in next-page
  // responses. Because time_usec is a required field, we can only serialize
  // this message if it is set.
  if (response_metadata.event_id().has_time_usec()) {
    result->stream_data.set_root_event_id(
        response_metadata.event_id().SerializeAsString());
  }
  result->stream_data.set_signed_in(!account_info.IsEmpty());
  result->stream_data.set_gaia(account_info.gaia);
  result->stream_data.set_email(account_info.email);
  result->stream_data.set_logging_enabled(
      chrome_response_metadata.logging_enabled());
  result->stream_data.set_privacy_notice_fulfilled(
      chrome_response_metadata.privacy_notice_fulfilled());

  for (const feedstore::Content& content : result->content) {
    feedstore::StreamContentHashList* hash_list = nullptr;
    for (auto& metadata : content.prefetch_metadata()) {
      if (!metadata.has_uri())
        continue;
      if (hash_list == nullptr)
        hash_list = result->stream_data.add_content_hashes();
      hash_list->add_hashes(
          feedstore::ContentHashFromPrefetchMetadata(metadata));
    }
  }

  std::optional<std::string> session_id = std::nullopt;
  if (!account_info.IsEmpty()) {
    // Signed-in requests don't use session_id tokens; set an empty value to
    // ensure that there are no old session_id tokens left hanging around.
    session_id = std::string();
  } else if (chrome_response_metadata.has_session_id()) {
    // Signed-out requests can set a new session token; otherwise, we leave
    // the default std::nullopt value to keep whatever token is already in
    // play.
    session_id = chrome_response_metadata.session_id();
  }

  std::optional<Experiments> experiments =
      TranslateExperiments(chrome_response_metadata.experiments());

  MetricsReporter::ActivityLoggingEnabled(
      chrome_response_metadata.logging_enabled());
  MetricsReporter::NoticeCardFulfilledObsolete(
      chrome_response_metadata.privacy_notice_fulfilled());

  RefreshResponseData response_data;
  response_data.model_update_request = std::move(result);
  response_data.request_schedule = std::move(global_data.request_schedule);
  response_data.content_lifetime = std::move(content_lifetime);
  response_data.session_id = std::move(session_id);
  response_data.experiments = std::move(experiments);
  response_data.server_request_received_timestamp =
      feedstore::FromTimestampMicros(
          feed_response->feed_response_metadata().event_id().time_usec());
  response_data.server_response_sent_timestamp = feedstore::FromTimestampMillis(
      feed_response->feed_response_metadata().response_time_ms());
  response_data.last_fetch_timestamp = current_time;
  response_data.web_and_app_activity_enabled =
      chrome_response_metadata.web_and_app_activity_enabled();
  response_data.discover_personalization_enabled =
      chrome_response_metadata.discover_personalization_enabled();

  return response_data;
}

std::vector<feedstore::DataOperation> TranslateDismissData(
    base::Time current_time,
    feedpacking::DismissData data) {
  std::vector<feedstore::DataOperation> result;
  for (auto& operation : data.data_operations()) {
    std::optional<feedstore::DataOperation> translated_operation =
        TranslateDataOperation(current_time, operation);
    if (translated_operation) {
      result.push_back(std::move(translated_operation.value()));
    }
  }
  return result;
}

}  // namespace feed
