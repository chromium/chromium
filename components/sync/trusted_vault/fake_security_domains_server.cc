// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/fake_security_domains_server.h"

#include "base/base64url.h"
#include "base/rand_util.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_crypto.h"
#include "components/sync/trusted_vault/trusted_vault_server_constants.h"

namespace syncer {

namespace {

const char kServerPathPrefix[] = "sds/";
const int kSharedKeyLength = 16;

std::unique_ptr<net::test_server::HttpResponse>
CreateHttpResponseForInvalidRequest() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_BAD_REQUEST);
  return response;
}

// Returns whether |request| satisfies protocol expectations.
bool ValidateJoinSecurityDomainsRequest(
    const sync_pb::JoinSecurityDomainsRequest& request) {
  if (request.security_domain().name() != kSyncSecurityDomainName) {
    DVLOG(1)
        << "JoinSecurityDomains request has unexpected security domain name: "
        << request.security_domain().name();
    return false;
  }

  const sync_pb::SecurityDomainMember& member =
      request.security_domain_member();
  if (member.public_key().empty()) {
    DVLOG(1) << "JoinSecurityDomains request has empty member public key";
    return false;
  }

  std::string encoded_public_key;
  base::Base64UrlEncode(member.public_key(),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);
  if (member.name() != kSecurityDomainMemberNamePrefix + encoded_public_key) {
    DVLOG(1) << "JoinSecurityDomains request has inconsistent member name ("
             << member.name() << ") and public key (" << member.public_key()
             << ")";
    return false;
  }

  const sync_pb::SharedMemberKey& shared_key = request.shared_member_key();
  if (shared_key.wrapped_key().empty()) {
    DVLOG(1)
        << "JoinSecurityDomains request has shared key with empty wrapped key";
    return false;
  }

  if (shared_key.member_proof().empty()) {
    DVLOG(1)
        << "JoinSecurityDomains request has shared key with empty wrapped key";
    return false;
  }

  return true;
}

std::string GetPublicKeyFromGetSecurityDomainMemberRequestURL(
    const GURL& request_url,
    const GURL& server_url) {
  const size_t start_pos =
      server_url.spec().size() + std::strlen(kSecurityDomainMemberNamePrefix);
  const size_t query_pos = request_url.spec().find('?');
  size_t length = 0;
  if (query_pos == std::string::npos) {
    // Query part isn't presented.
    length = std::string::npos;
  } else {
    length = query_pos - start_pos;
  }
  const std::string encoded_public_key =
      request_url.spec().substr(start_pos, length);
  std::string decoded_public_key;
  if (!base::Base64UrlDecode(encoded_public_key,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &decoded_public_key)) {
    return std::string();
  }
  return decoded_public_key;
}

}  // namespace

FakeSecurityDomainsServer::FakeSecurityDomainsServer(GURL base_url)
    : server_url_(base_url.spec() + kServerPathPrefix) {}

FakeSecurityDomainsServer::~FakeSecurityDomainsServer() = default;

void FakeSecurityDomainsServer::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeSecurityDomainsServer::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::unique_ptr<net::test_server::HttpResponse>
FakeSecurityDomainsServer::HandleRequest(
    const net::test_server::HttpRequest& http_request) {
  DVLOG(1) << "Received request";
  if (!base::StartsWith(http_request.GetURL().spec(), server_url_.spec())) {
    // This request shouldn't be handled by security domains server.
    return nullptr;
  }

  std::unique_ptr<net::test_server::HttpResponse> response;
  if (http_request.GetURL() ==
      GetFullJoinSecurityDomainsURLForTesting(server_url_)) {
    response = HandleJoinSecurityDomainsRequest(http_request);
  } else if (base::StartsWith(
                 http_request.GetURL().spec(),
                 server_url_.spec() + kSecurityDomainMemberNamePrefix)) {
    response = HandleGetSecurityDomainMemberRequest(http_request);
  } else {
    DVLOG(1) << "Unknown request url: " << http_request.GetURL().spec();
    received_invalid_request_ = true;
    response = CreateHttpResponseForInvalidRequest();
  }

  for (auto& observer : observers_) {
    observer.OnRequestHandled();
  }
  return response;
}

std::vector<uint8_t> FakeSecurityDomainsServer::RotateTrustedVaultKey(
    const std::vector<uint8_t>& last_trusted_vault_key) {
  std::vector<uint8_t> new_trusted_vault_key(kSharedKeyLength);
  base::RandBytes(new_trusted_vault_key.data(), kSharedKeyLength);

  current_epoch_++;
  constant_key_allowed_ = false;
  for (auto& member_and_shared_key : public_key_to_shared_keys_) {
    std::unique_ptr<SecureBoxPublicKey> member_public_key =
        SecureBoxPublicKey::CreateByImport(
            ProtoStringToBytes(member_and_shared_key.first));
    DCHECK(member_public_key);

    sync_pb::SharedMemberKey new_shared_key;
    new_shared_key.set_epoch(current_epoch_);
    AssignBytesToProtoString(ComputeTrustedVaultWrappedKey(
                                 *member_public_key, new_trusted_vault_key),
                             new_shared_key.mutable_wrapped_key());
    AssignBytesToProtoString(
        ComputeTrustedVaultHMAC(
            /*key=*/new_trusted_vault_key,
            /*data=*/ProtoStringToBytes(member_and_shared_key.first)),
        new_shared_key.mutable_member_proof());
    member_and_shared_key.second.push_back(new_shared_key);

    sync_pb::RotationProof rotation_proof;
    rotation_proof.set_new_epoch(current_epoch_);
    AssignBytesToProtoString(
        ComputeTrustedVaultHMAC(/*key=*/last_trusted_vault_key,
                                /*data=*/new_trusted_vault_key),
        rotation_proof.mutable_rotation_proof());
    public_key_to_rotation_proofs_[member_and_shared_key.first].push_back(
        rotation_proof);
  }

  return new_trusted_vault_key;
}

int FakeSecurityDomainsServer::GetMemberCount() const {
  return public_key_to_shared_keys_.size();
}

bool FakeSecurityDomainsServer::AllMembersHaveKey(
    const std::vector<uint8_t>& trusted_vault_key) const {
  for (const auto& public_key_and_shared_keys : public_key_to_shared_keys_) {
    bool member_has_key = false;
    for (const auto& shared_key : public_key_and_shared_keys.second) {
      // Member has |trusted_vault_key| if there is a member proof signed by
      // |trusted_vault_key|.
      if (VerifyTrustedVaultHMAC(
              /*key=*/trusted_vault_key,
              /*data=*/ProtoStringToBytes(public_key_and_shared_keys.first),
              /*digest=*/ProtoStringToBytes(shared_key.member_proof()))) {
        member_has_key = true;
        break;
      }
    }
    if (!member_has_key) {
      return false;
    }
  }
  return true;
}

std::unique_ptr<net::test_server::HttpResponse>
FakeSecurityDomainsServer::HandleJoinSecurityDomainsRequest(
    const net::test_server::HttpRequest& http_request) {
  if (http_request.method != net::test_server::METHOD_POST) {
    DVLOG(1) << "JoinSecurityDomains request has wrong method: "
             << http_request.method;
    received_invalid_request_ = true;
    return CreateHttpResponseForInvalidRequest();
  }
  // TODO(crbug.com/1113599): consider verifying content type and access token
  // headers.

  sync_pb::JoinSecurityDomainsRequest deserialized_content;
  if (!deserialized_content.ParseFromString(http_request.content)) {
    DVLOG(1) << "Failed to deserialize JoinSecurityDomains request content";
    received_invalid_request_ = true;
    return CreateHttpResponseForInvalidRequest();
  }

  if (!ValidateJoinSecurityDomainsRequest(deserialized_content)) {
    received_invalid_request_ = true;
    return CreateHttpResponseForInvalidRequest();
  }

  const sync_pb::SecurityDomainMember& member =
      deserialized_content.security_domain_member();
  if (public_key_to_shared_keys_.count(member.public_key()) != 0) {
    // Member already exists.
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_PRECONDITION_FAILED);
    return response;
  }

  const sync_pb::SharedMemberKey& shared_key =
      deserialized_content.shared_member_key();
  if (shared_key.has_epoch() && shared_key.epoch() != current_epoch_) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_PRECONDITION_FAILED);
    return response;
  }

  if (shared_key.has_epoch()) {
    // Valid joining of existing security domain.
    public_key_to_shared_keys_[member.public_key()] = {shared_key};
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    return response;
  }

  if (!constant_key_allowed_ ||
      !VerifyTrustedVaultHMAC(
          /*key=*/GetConstantTrustedVaultKey(),
          /*data=*/ProtoStringToBytes(member.public_key()),
          /*digest=*/ProtoStringToBytes(shared_key.member_proof()))) {
    // Either constant key is not allowed, or request uses the real key without
    // populating the epoch.
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_PRECONDITION_FAILED);
    return response;
  }

  // Valid joining with constant key.
  if (current_epoch_ == 0) {
    // Simulate generation of random epoch when security domain just created.
    DCHECK(public_key_to_shared_keys_.empty());
    current_epoch_ = 100;
  }

  public_key_to_shared_keys_[member.public_key()] = {shared_key};
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  return response;
}

std::unique_ptr<net::test_server::HttpResponse>
FakeSecurityDomainsServer::HandleGetSecurityDomainMemberRequest(
    const net::test_server::HttpRequest& http_request) {
  GURL url = http_request.GetURL();
  const std::string member_public_key =
      GetPublicKeyFromGetSecurityDomainMemberRequestURL(http_request.GetURL(),
                                                        server_url_);
  if (member_public_key.empty()) {
    DVLOG(1) << "Member public key isn't presented or corrupted in "
                "GetSecurityDomainMemberRequest";
    return CreateHttpResponseForInvalidRequest();
  }
  if (public_key_to_shared_keys_.count(member_public_key) == 0) {
    DVLOG(1) << "Member requested in GetSecurityDomainMemberRequest not found";
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_NOT_FOUND);
    return response;
  }
  sync_pb::SecurityDomainMember member;

  std::string encoded_public_key;
  base::Base64UrlEncode(member_public_key,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);

  member.set_name(kSecurityDomainMemberNamePrefix + encoded_public_key);
  member.set_public_key(member_public_key);

  sync_pb::SecurityDomainMember::SecurityDomainMembership* membership =
      member.add_memberships();
  membership->set_security_domain(kSyncSecurityDomainName);
  for (const auto& shared_key : public_key_to_shared_keys_[member_public_key]) {
    *membership->add_keys() = shared_key;
  }
  for (const auto& rotation_proof :
       public_key_to_rotation_proofs_[member_public_key]) {
    *membership->add_rotation_proofs() = rotation_proof;
  }

  member.set_member_type(
      sync_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE);
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(member.SerializeAsString());
  return response;
}

}  // namespace syncer
