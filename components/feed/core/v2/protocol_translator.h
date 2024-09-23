// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PROTOCOL_TRANSLATOR_H_
#define COMPONENTS_FEED_CORE_V2_PROTOCOL_TRANSLATOR_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "components/feed/core/proto/v2/packing.pb.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/data_operation.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/types.h"

namespace feed {

// Data for updating StreamModel. This can be sourced from the network or
// persistent storage.
struct StreamModelUpdateRequest {
 public:
  enum class Source {
    kNetworkUpdate,
    kInitialLoadFromStore,
    kNetworkLoadMore,
  };

  StreamModelUpdateRequest();
  ~StreamModelUpdateRequest();
  StreamModelUpdateRequest(const StreamModelUpdateRequest&);
  StreamModelUpdateRequest& operator=(const StreamModelUpdateRequest&);

  // Whether this data originates is from the initial load of content from
  // the local data store.
  Source source = Source::kNetworkUpdate;

  // The set of Contents marked UPDATE_OR_APPEND in the response, in the order
  // in which they were received.
  std::vector<feedstore::Content> content;

  // Contains the root ContentId, tokens, a timestamp for when the most recent
  // content was added, and a list of ContentIds for clusters in the response.
  feedstore::StreamData stream_data;

  // The set of StreamSharedStates marked UPDATE_OR_APPEND in the order in which
  // they were received.
  std::vector<feedstore::StreamSharedState> shared_states;

  std::vector<feedstore::StreamStructure> stream_structures;

  int32_t max_structure_sequence_number = 0;
};

struct RefreshResponseData {
  RefreshResponseData();
  ~RefreshResponseData();
  RefreshResponseData(RefreshResponseData&&);
  RefreshResponseData& operator=(RefreshResponseData&&);

  std::unique_ptr<StreamModelUpdateRequest> model_update_request;

  // Server-defined request schedule, if provided.
  std::optional<RequestSchedule> request_schedule;

  // Server-defined content lifetime, if provided.
  std::optional<feedstore::Metadata::StreamMetadata::ContentLifetime>
      content_lifetime;

  // Server-defined session id token, if provided.
  std::optional<std::string> session_id;

  // List of experiments from the server, if provided.
  std::optional<Experiments> experiments;

  // Server-reported network timestamps. They can be compared to
  // each other but not to client timestamps.
  base::Time server_request_received_timestamp;
  base::Time server_response_sent_timestamp;

  // The client-side timestamp that the response is fetched.
  base::Time last_fetch_timestamp;

  bool web_and_app_activity_enabled = false;
  bool discover_personalization_enabled = false;
};

std::optional<feedstore::DataOperation> TranslateDataOperation(
    base::Time current_time,
    feedwire::DataOperation wire_operation);

RefreshResponseData TranslateWireResponse(
    feedwire::Response response,
    StreamModelUpdateRequest::Source source,
    const AccountInfo& account_info,
    base::Time current_time);

std::vector<feedstore::DataOperation> TranslateDismissData(
    base::Time current_time,
    feedpacking::DismissData data);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PROTOCOL_TRANSLATOR_H_
