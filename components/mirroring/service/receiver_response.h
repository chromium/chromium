// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_RECEIVER_RESPONSE_H_
#define COMPONENTS_MIRRORING_SERVICE_RECEIVER_RESPONSE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/component_export.h"
#include "third_party/jsoncpp/source/include/json/value.h"
#include "third_party/openscreen/src/cast/streaming/answer_messages.h"

namespace mirroring {

// Receiver response message type.
enum class ResponseType {
  UNKNOWN,
  ANSWER,                 // Response to OFFER message.
  CAPABILITIES_RESPONSE,  // Response to GET_CAPABILITIES message.
  RPC,                    // Rpc binary messages. The payload is base64 encoded.
};

struct COMPONENT_EXPORT(MIRRORING_SERVICE) ReceiverCapability {
  ReceiverCapability();
  ~ReceiverCapability();
  ReceiverCapability(ReceiverCapability&& receiver_response);
  ReceiverCapability(const ReceiverCapability& receiver_response);
  ReceiverCapability& operator=(ReceiverCapability&& receiver_response);
  ReceiverCapability& operator=(const ReceiverCapability& receiver_response);

  static constexpr int kRemotingVersionUnknown = -1;

  // The remoting version that the receiver uses.
  int remoting = kRemotingVersionUnknown;

  // Set of capabilities (e.g., ac3, 4k, hevc, vp9, dolby_vision, etc.).
  std::vector<std::string> media_caps;
};

struct COMPONENT_EXPORT(MIRRORING_SERVICE) ReceiverError {
  ReceiverError();
  ~ReceiverError();
  ReceiverError(ReceiverError&& receiver_response);
  ReceiverError(const ReceiverError& receiver_response);
  ReceiverError& operator=(ReceiverError&& receiver_response);
  ReceiverError& operator=(const ReceiverError& receiver_response);

  int32_t code = -1;
  std::string description;
  std::string details;  // In JSON format.
};

// TODO(b/160978984): Migrate parsing and ReceiverResponse object to libcast.
class COMPONENT_EXPORT(MIRRORING_SERVICE) ReceiverResponse {
 public:
  ReceiverResponse();
  ~ReceiverResponse();

  // ReceiverResponse is shallow move only due to having unique_ptrs. We expose
  // a clone method for testing that performs a deep copy.
  ReceiverResponse(ReceiverResponse&& receiver_response);
  ReceiverResponse(const ReceiverResponse& receiver_response) = delete;
  ReceiverResponse& operator=(ReceiverResponse&& receiver_response);
  ReceiverResponse& operator=(const ReceiverResponse& receiver_response) =
      delete;

  static std::unique_ptr<ReceiverResponse> Parse(
      const std::string& message_data);
  static ReceiverResponse CreateErrorResponse();

  // Test only methods
  std::unique_ptr<ReceiverResponse> CloneForTesting() const;
  static ReceiverResponse CreateAnswerResponseForTesting(
      int32_t sequence_number,
      std::unique_ptr<openscreen::cast::Answer> answer);
  static ReceiverResponse CreateCapabilitiesResponseForTesting(
      int32_t sequence_number,
      std::unique_ptr<ReceiverCapability> capabilities);

  // Simple getter for the ResponseType. Note that if the message is an error
  // message, this will be UNKNOWN since it's technically not applicable.
  ResponseType type() const { return type_; }

  // All messages have same |session_id| for each mirroring session. This value
  // is provided by the media router provider.
  int32_t session_id() const { return session_id_; }

  // This should be same as the value in the corresponding query/OFFER messages
  // for non-rpc messages.
  int sequence_number() const { return sequence_number_; }

  // We don't expose "result" directly, to avoid string comparisons to "ok"
  // and "error." This method returns true if and only if the result is "ok"
  // and the object its specified payload.
  bool valid() const { return valid_; }

  // Each response type has its own payload, including errors. If the receiver
  // response exists, the payload is guaranteed to be present based on the
  // DCHECK rules below.
  const openscreen::cast::Answer& answer() const {
    DCHECK(valid_ && type_ == ResponseType::ANSWER);
    return *answer_;
  }

  const std::string& rpc() const {
    DCHECK(valid_ && type_ == ResponseType::RPC);
    return rpc_;
  }

  const ReceiverCapability& capabilities() const {
    DCHECK(valid_ && type_ == ResponseType::CAPABILITIES_RESPONSE);
    return *capabilities_;
  }

  // Errors are different than the other payloads, and may be null depending
  // on the type of error.
  const ReceiverError* error() const {
    DCHECK(!valid_);
    return error_.get();
  }

 private:
  ResponseType type_ = ResponseType::UNKNOWN;
  int32_t session_id_ = -1;
  int32_t sequence_number_ = -1;
  bool valid_ = false;

  // Only one of these fields will be populated, based on the ResponseType
  // value. Currently, we enforce this by DCHECKing on their accessors, instead
  // of subclasses or a sum type/union/variant. ResponseType associated with
  // each type is documented below.

  // ResponseType::ANSWER
  std::unique_ptr<openscreen::cast::Answer> answer_;

  // ResponseType::RPC
  // Contains the decoded (i.e. raw binary) RPC data.
  std::string rpc_;

  // ResponseType::CAPABILITIES_RESPONSE
  std::unique_ptr<ReceiverCapability> capabilities_;

  // Error may be populated for any ResponseType, as long as valid_ = false.
  std::unique_ptr<ReceiverError> error_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_RECEIVER_RESPONSE_H_
