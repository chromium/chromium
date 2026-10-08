// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/fake_ephemeral_key_server.h"

#include <utility>
#include <vector>

#include "base/containers/map_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/protocol/encrypted_tab_context_item_specifics.pb.h"
#include "components/sync/protocol/tab_context_container_access_token.pb.h"
#include "components/sync_tab_context/proto/ephemeral_key_service.pb.h"
#include "net/http/http_request_headers.h"
#include "third_party/zlib/google/compression_utils.h"

namespace sync_tab_context {

namespace {

constexpr std::string_view kServerPath = "v1/ephemeralKeys";
constexpr std::string_view kBearerPrefix = "Bearer ";

std::unique_ptr<net::test_server::HttpResponse> CreateErrorResponse(
    net::HttpStatusCode response_code) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(response_code);
  return response;
}

}  // namespace

// static
GURL FakeEphemeralKeyServer::GetServerURL(const GURL& base_url) {
  return base_url.Resolve(kServerPath);
}

FakeEphemeralKeyServer::FakeEphemeralKeyServer(const GURL& base_url)
    : server_url_(GetServerURL(base_url)) {}

FakeEphemeralKeyServer::~FakeEphemeralKeyServer() = default;

std::unique_ptr<net::test_server::HttpResponse>
FakeEphemeralKeyServer::HandleRequest(
    const net::test_server::HttpRequest& http_request) {
  if (http_request.GetURL() != server_url_) {
    return nullptr;
  }

  if (http_request.method != net::test_server::METHOD_POST) {
    return CreateErrorResponse(net::HTTP_BAD_REQUEST);
  }

  const std::string* const auth_header = base::FindOrNull(
      http_request.headers, net::HttpRequestHeaders::kAuthorization);
  if (!auth_header || !base::StartsWith(*auth_header, kBearerPrefix) ||
      auth_header->size() <= kBearerPrefix.size()) {
    return CreateErrorResponse(net::HTTP_UNAUTHORIZED);
  }

  GenerateEphemeralKeyRequest request_proto;
  if (!request_proto.ParseFromString(http_request.content)) {
    return CreateErrorResponse(net::HTTP_BAD_REQUEST);
  }

  const std::optional<net::HttpStatusCode> http_error =
      http_error_to_return_.load();
  if (http_error.has_value()) {
    return CreateErrorResponse(*http_error);
  }

  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set =
      syncer::AgileSymmetricKeySet::CreateEmpty();
  key_set->RotatePrimaryToNewlyGeneratedRandomKey();
  const sync_pb::AgileSymmetricKeySet key_set_proto = key_set->ToProto();

  std::string name;
  {
    base::AutoLock auto_lock(lock_);
    name = base::StrCat(
        {"ephemeralKeys/", base::NumberToString(++next_server_token_id_)});
    issued_key_sets_[name] = key_set_proto;
  }

  GenerateEphemeralKeyResponse response_proto;
  response_proto.set_name(name);
  *response_proto.mutable_agile_symmetric_key_set() = key_set_proto;
  response_proto.mutable_expire_time()->set_seconds(
      (base::Time::Now() + base::Hours(1) - base::Time::UnixEpoch())
          .InSeconds());

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(response_proto.SerializeAsString());
  response->set_content_type("application/x-protobuf");
  return response;
}

void FakeEphemeralKeyServer::SetHttpErrorToReturn(
    std::optional<net::HttpStatusCode> http_error) {
  http_error_to_return_.store(http_error);
}

std::optional<std::string> FakeEphemeralKeyServer::DecryptContent(
    std::string_view serialized_access_token,
    const sync_pb::EncryptedData& encrypted_content) const {
  sync_pb::TabContextContainerAccessToken token_proto;
  if (!token_proto.ParseFromString(serialized_access_token)) {
    return std::nullopt;
  }

  std::unique_ptr<syncer::AgileSymmetricKeySet> ephemeral_key_set;
  {
    base::AutoLock auto_lock(lock_);
    const sync_pb::AgileSymmetricKeySet* const issued_key_set =
        base::FindOrNull(issued_key_sets_, token_proto.name());
    if (!issued_key_set) {
      return std::nullopt;
    }
    ephemeral_key_set =
        syncer::AgileSymmetricKeySet::FromProto(*issued_key_set);
  }
  if (!ephemeral_key_set) {
    return std::nullopt;
  }

  sync_pb::EncryptedData encrypted_container_key;
  if (!encrypted_container_key.ParseFromString(
          token_proto.encrypted_container_key())) {
    return std::nullopt;
  }

  const std::optional<std::vector<uint8_t>> decrypted_key_set_bytes =
      ephemeral_key_set->Decrypt(encrypted_container_key);
  if (!decrypted_key_set_bytes.has_value()) {
    return std::nullopt;
  }

  sync_pb::AgileSymmetricKeySet container_key_set_proto;
  if (!container_key_set_proto.ParseFromArray(
          decrypted_key_set_bytes->data(), decrypted_key_set_bytes->size())) {
    return std::nullopt;
  }

  const std::unique_ptr<syncer::AgileSymmetricKeySet> container_key_set =
      syncer::AgileSymmetricKeySet::FromProto(container_key_set_proto);
  if (!container_key_set) {
    return std::nullopt;
  }

  const std::optional<std::vector<uint8_t>> decrypted_content_bytes =
      container_key_set->Decrypt(encrypted_content);
  if (!decrypted_content_bytes.has_value()) {
    return std::nullopt;
  }

  sync_pb::TabContextItemContent item_content;
  if (!item_content.ParseFromArray(decrypted_content_bytes->data(),
                                   decrypted_content_bytes->size())) {
    return std::nullopt;
  }

  switch (item_content.content_case()) {
    case sync_pb::TabContextItemContent::kRawData:
      return item_content.raw_data();
    case sync_pb::TabContextItemContent::kGzipCompressedData: {
      std::string uncompressed_data;
      if (!compression::GzipUncompress(item_content.gzip_compressed_data(),
                                       &uncompressed_data)) {
        return std::nullopt;
      }
      return uncompressed_data;
    }
    case sync_pb::TabContextItemContent::CONTENT_NOT_SET:
      return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace sync_tab_context
