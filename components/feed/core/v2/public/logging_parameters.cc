// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/logging_parameters.h"

#include "base/logging.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/types.h"

namespace feed {

LoggingParameters::LoggingParameters() = default;
LoggingParameters::~LoggingParameters() = default;
LoggingParameters::LoggingParameters(const LoggingParameters&) = default;
LoggingParameters::LoggingParameters(LoggingParameters&&) = default;
LoggingParameters& LoggingParameters::operator=(const LoggingParameters&) =
    default;

bool LoggingParameters::operator==(const LoggingParameters& rhs) const {
  return std::tie(email, client_instance_id, logging_enabled,
                  view_actions_enabled, root_event_id) ==
         std::tie(rhs.email, rhs.client_instance_id, logging_enabled,
                  view_actions_enabled, root_event_id);
}

LoggingParameters MakeLoggingParameters(
    const std::string client_instance_id,
    const StreamModelUpdateRequest& update_request) {
  const feedstore::StreamData& stream_data = update_request.stream_data;
  bool signed_in = stream_data.signed_in() && !stream_data.email().empty() &&
                   !stream_data.gaia().empty();

  LoggingParameters logging_params;
  logging_params.client_instance_id = client_instance_id;
  logging_params.root_event_id = stream_data.root_event_id();
  logging_params.logging_enabled =
      !(feedstore::StreamTypeFromKey(stream_data.stream_key())
            .IsSingleWebFeedEntryMenu()) &&
      (((signed_in && stream_data.logging_enabled()) ||
        (!signed_in && GetFeedConfig().send_signed_out_session_logs)));
  logging_params.view_actions_enabled = logging_params.logging_enabled;

  if (signed_in) {
    DCHECK(!stream_data.email().empty());
    DCHECK(!stream_data.gaia().empty());
    // We provide account name even if logging is disabled, so that account
    // name can be verified for action uploads.
    logging_params.email = stream_data.email();
  }

  return logging_params;
}

LoggingParameters FromProto(const feedui::LoggingParameters& proto) {
  LoggingParameters result;
  result.email = proto.email();
  result.client_instance_id = proto.client_instance_id();
  result.logging_enabled = proto.logging_enabled();
  result.view_actions_enabled = proto.view_actions_enabled();
  result.root_event_id = proto.root_event_id();
  return result;
}

void ToProto(const LoggingParameters& logging_parameters,
             feedui::LoggingParameters& proto) {
  proto.set_email(logging_parameters.email);
  proto.set_client_instance_id(logging_parameters.client_instance_id);
  proto.set_logging_enabled(logging_parameters.logging_enabled);
  proto.set_view_actions_enabled(logging_parameters.view_actions_enabled);
  proto.set_root_event_id(logging_parameters.root_event_id);
}

}  // namespace feed
