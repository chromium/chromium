// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/build_container_access_token.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/tab_context_container_access_token.pb.h"

namespace sync_tab_context {

namespace {

google::protobuf::Timestamp TimeToGoogleTimestampProto(base::Time time) {
  google::protobuf::Timestamp timestamp;
  const int64_t seconds = (time - base::Time::UnixEpoch()).InSeconds();
  const int64_t nanos =
      ((time - base::Time::UnixEpoch()) - base::Seconds(seconds))
          .InNanoseconds();
  timestamp.set_seconds(seconds);
  timestamp.set_nanos(static_cast<int32_t>(nanos));
  return timestamp;
}

}  // namespace

std::optional<std::string> BuildContainerAccessToken(
    const syncer::AgileSymmetricKeySet& container_key_set,
    EphemeralKeyFetcher::Result ephemeral_key_result) {
  CHECK(ephemeral_key_result.ephemeral_key);

  const std::string container_key_bytes =
      container_key_set.ToProto().SerializeAsString();
  const std::optional<sync_pb::EncryptedData> encrypted_container_key =
      ephemeral_key_result.ephemeral_key->Encrypt(
          base::as_byte_span(container_key_bytes));
  if (!encrypted_container_key) {
    return std::nullopt;
  }

  sync_pb::TabContextContainerAccessToken token_proto;
  token_proto.set_name(std::move(ephemeral_key_result.name));
  token_proto.set_encrypted_container_key(
      encrypted_container_key->SerializeAsString());
  if (!ephemeral_key_result.expire_time.is_null()) {
    *token_proto.mutable_expire_time() =
        TimeToGoogleTimestampProto(ephemeral_key_result.expire_time);
  }

  return token_proto.SerializeAsString();
}

}  // namespace sync_tab_context
