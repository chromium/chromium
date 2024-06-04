// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/test/fake_security_domains_server.h"

#include "base/base64url.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_crypto.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "net/http/http_status_code.h"

namespace trusted_vault {

namespace {

const char kServerPathPrefix[] = "sds/";
const int kSharedKeyLength = 16;

std::unique_ptr<net::test_server::HttpResponse> CreateErrorResponse(
    net::HttpStatusCode response_code) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(response_code);
  return response;
}

std::unique_ptr<net::test_server::HttpResponse>
CreateHttpResponseForSuccessfulJoinSecurityDomainsRequest(int current_epoch) {
  trusted_vault_pb::JoinSecurityDomainsResponse response_proto;
  trusted_vault_pb::SecurityDomain* security_domain =
      response_proto.mutable_security_domain();
  security_domain->set_name(
      GetSecurityDomainPath(SecurityDomainId::kChromeSync));
  security_domain->set_current_epoch(current_epoch);

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(response_proto.SerializeAsString());
  return response;
}

// Returns whether |request| satisfies protocol expectations.
bool ValidateJoinSecurityDomainsRequest(
    const trusted_vault_pb::JoinSecurityDomainsRequest& request) {
  const std::string expected_name =
      GetSecurityDomainPath(SecurityDomainId::kChromeSync);
  if (request.security_domain().name() != expected_name) {
    DVLOG(1)
        << "JoinSecurityDomains request has unexpected security domain name: "
        << request.security_domain().name();
    return false;
  }

  const trusted_vault_pb::SecurityDomainMember& member =
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

  if (!SecureBoxPublicKey::CreateByImport(
          ProtoStringToBytes(member.public_key()))) {
    DVLOG(1) << "JoinSecurityDomains request has invalid public key: "
             << member.public_key();
    return false;
  }

  if (request.shared_member_key().empty()) {
    DVLOG(1) << "JoinSecurityDomains request has no shared keys";
    return false;
  }
  for (const trusted_vault_pb::SharedMemberKey& shared_key :
       request.shared_member_key()) {
    if (shared_key.wrapped_key().empty()) {
      DVLOG(1) << "JoinSecurityDomains request has shared key with empty "
                  "wrapped key";
      return false;
    }

    if (shared_key.member_proof().empty()) {
      DVLOG(1) << "JoinSecurityDomains request has shared key with empty "
                  "wrapped key";
      return false;
    }
  }

  if (member.member_type() !=
          trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_UNSPECIFIED &&
      request.member_type_hint() != 0 &&
      static_cast<int>(member.member_type()) != request.member_type_hint()) {
    DVLOG(1) << "JoinSecurityDomains request has inconsistent member type hint";
    return false;
  }

  return true;
}

// Verifies that shared keys passed as part of the |request| match
// |trusted_vault_keys|.
bool VerifySharedKeys(
    const trusted_vault_pb::JoinSecurityDomainsRequest& request,
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys) {
  if (request.shared_member_key_size() !=
      static_cast<int>(trusted_vault_keys.size())) {
    return false;
  }

  std::unique_ptr<SecureBoxPublicKey> member_public_key =
      SecureBoxPublicKey::CreateByImport(
          ProtoStringToBytes(request.security_domain_member().public_key()));
  DCHECK(member_public_key);
  for (size_t i = 0; i < trusted_vault_keys.size(); ++i) {
    if (!VerifyMemberProof(
            *member_public_key, trusted_vault_keys[i],
            ProtoStringToBytes(request.shared_member_key(i).member_proof()))) {
      return false;
    }
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

FakeSecurityDomainsServer::State::State() = default;

FakeSecurityDomainsServer::State::State(State&& other) = default;

FakeSecurityDomainsServer::State& FakeSecurityDomainsServer::State::operator=(
    State&& other) = default;

FakeSecurityDomainsServer::State::~State() = default;

// static
GURL FakeSecurityDomainsServer::GetServerURL(GURL base_url) {
  return GURL(base_url.spec() + kServerPathPrefix);
}

FakeSecurityDomainsServer::FakeSecurityDomainsServer(GURL base_url)
    : server_url_(GetServerURL(base_url)),
      observers_(
          base::MakeRefCounted<base::ObserverListThreadSafe<Observer>>()) {
  state_.trusted_vault_keys.push_back(GetConstantTrustedVaultKey());
}

FakeSecurityDomainsServer::~FakeSecurityDomainsServer() = default;

void FakeSecurityDomainsServer::AddObserver(Observer* observer) {
  observers_->AddObserver(observer);
}

void FakeSecurityDomainsServer::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
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
      GetFullJoinSecurityDomainsURLForTesting(server_url_,
                                              SecurityDomainId::kChromeSync)) {
    response = HandleJoinSecurityDomainsRequest(http_request);
  } else if (base::StartsWith(
                 http_request.GetURL().spec(),
                 server_url_.spec() + kSecurityDomainMemberNamePrefix)) {
    response = HandleGetSecurityDomainMemberRequest(http_request);
  } else if (http_request.GetURL() ==
             GetFullGetSecurityDomainURLForTesting(
                 server_url_, SecurityDomainId::kChromeSync)) {
    response = HandleGetSecurityDomainRequest(http_request);
  } else {
    base::AutoLock autolock(lock_);
    DVLOG(1) << "Unknown request url: " << http_request.GetURL().spec();
    state_.received_invalid_request = true;
    response = CreateErrorResponse(net::HTTP_NOT_FOUND);
  }

  observers_->Notify(FROM_HERE, &Observer::OnRequestHandled);
  return response;
}

std::vector<uint8_t> FakeSecurityDomainsServer::RotateTrustedVaultKey(
    const std::vector<uint8_t>& last_trusted_vault_key) {
  base::AutoLock autolock(lock_);
  std::vector<uint8_t> new_trusted_vault_key(kSharedKeyLength);
  base::RandBytes(new_trusted_vault_key);

  state_.current_epoch++;
  state_.trusted_vault_keys.push_back(new_trusted_vault_key);
  state_.constant_key_allowed = false;
  for (auto& [member, shared_key] : state_.public_key_to_shared_keys) {
    std::unique_ptr<SecureBoxPublicKey> member_public_key =
        SecureBoxPublicKey::CreateByImport(ProtoStringToBytes(member));
    DCHECK(member_public_key);

    trusted_vault_pb::SharedMemberKey new_shared_key;
    new_shared_key.set_epoch(state_.current_epoch);
    AssignBytesToProtoString(ComputeTrustedVaultWrappedKey(
                                 *member_public_key, new_trusted_vault_key),
                             new_shared_key.mutable_wrapped_key());
    AssignBytesToProtoString(
        ComputeMemberProof(*member_public_key, new_trusted_vault_key),
        new_shared_key.mutable_member_proof());
    shared_key.push_back(new_shared_key);

    trusted_vault_pb::RotationProof rotation_proof;
    rotation_proof.set_new_epoch(state_.current_epoch);
    AssignBytesToProtoString(
        ComputeRotationProofForTesting(
            /*trusted_vault_key=*/new_trusted_vault_key,
            /*prev_trusted_vault_key=*/last_trusted_vault_key),
        rotation_proof.mutable_rotation_proof());
    state_.public_key_to_rotation_proofs[member].push_back(rotation_proof);
  }

  return new_trusted_vault_key;
}

void FakeSecurityDomainsServer::ResetData() {
  base::AutoLock autolock(lock_);
  state_ = State();
  state_.trusted_vault_keys.push_back(GetConstantTrustedVaultKey());
}

void FakeSecurityDomainsServer::ResetDataToState(
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  CHECK(!keys.empty());
  CHECK(keys.size() > 1 || keys.front() != GetConstantTrustedVaultKey());
  base::AutoLock autolock(lock_);
  state_ = State();
  state_.trusted_vault_keys = keys;
  state_.constant_key_allowed = false;
  state_.current_epoch = last_key_version;
}

void FakeSecurityDomainsServer::RequirePublicKeyToAvoidRecoverabilityDegraded(
    const std::vector<uint8_t>& public_key) {
  DCHECK(!public_key.empty());
  base::AutoLock autolock(lock_);
  AssignBytesToProtoString(
      public_key, &state_.required_public_key_to_avoid_recoverability_degraded);
}

int FakeSecurityDomainsServer::GetMemberCount() const {
  base::AutoLock autolock(lock_);
  return state_.public_key_to_shared_keys.size();
}

std::vector<std::vector<uint8_t>>
FakeSecurityDomainsServer::GetAllTrustedVaultKeys() const {
  base::AutoLock autolock(lock_);
  return state_.trusted_vault_keys;
}

bool FakeSecurityDomainsServer::AllMembersHaveKey(
    const std::vector<uint8_t>& trusted_vault_key) const {
  base::AutoLock autolock(lock_);
  for (const auto& [public_key, shared_keys] :
       state_.public_key_to_shared_keys) {
    bool member_has_key = false;

    std::unique_ptr<SecureBoxPublicKey> member_public_key =
        SecureBoxPublicKey::CreateByImport(ProtoStringToBytes(public_key));
    DCHECK(member_public_key);

    for (const trusted_vault_pb::SharedMemberKey& shared_key : shared_keys) {
      // Member has |trusted_vault_key| if there is a member proof signed by
      // |trusted_vault_key|.
      if (VerifyMemberProof(*member_public_key, trusted_vault_key,
                            ProtoStringToBytes(shared_key.member_proof()))) {
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

int FakeSecurityDomainsServer::GetCurrentEpoch() const {
  base::AutoLock autolock(lock_);
  return state_.current_epoch;
}

bool FakeSecurityDomainsServer::IsRecoverabilityDegraded() const {
  base::AutoLock autolock(lock_);
  if (state_.required_public_key_to_avoid_recoverability_degraded.empty()) {
    return false;
  }

  return state_.public_key_to_shared_keys.count(
             state_.required_public_key_to_avoid_recoverability_degraded) == 0;
}

bool FakeSecurityDomainsServer::ReceivedInvalidRequest() const {
  base::AutoLock autolock(lock_);
  return state_.received_invalid_request;
}

std::unique_ptr<net::test_server::HttpResponse>
FakeSecurityDomainsServer::HandleJoinSecurityDomainsRequest(
    const net::test_server::HttpRequest& http_request) {
  base::AutoLock autolock(lock_);
  if (http_request.method != net::test_server::METHOD_POST) {
    DVLOG(1) << "JoinSecurityDomains request has wrong method: "
             << http_request.method;
    state_.received_invalid_request = true;
    return CreateErrorResponse(net::HTTP_INTERNAL_SERVER_ERROR);
  }
  // TODO(crbug.com/40143545): consider verifying content type and access token
  // headers.

  trusted_vault_pb::JoinSecurityDomainsRequest deserialized_content;
  if (!deserialized_content.ParseFromString(http_request.content)) {
    DVLOG(1) << "Failed to deserialize JoinSecurityDomains request content";
    state_.received_invalid_request = true;
    return CreateErrorResponse(net::HTTP_INTERNAL_SERVER_ERROR);
  }

  if (!ValidateJoinSecurityDomainsRequest(deserialized_content)) {
    state_.received_invalid_request = true;
    return CreateErrorResponse(net::HTTP_INTERNAL_SERVER_ERROR);
  }

  const trusted_vault_pb::SecurityDomainMember& member =
      deserialized_content.security_domain_member();
  if (state_.public_key_to_shared_keys.count(member.public_key()) != 0) {
    // Member already exists.
    return CreateErrorResponse(net::HTTP_CONFLICT);
  }

  int last_shared_key_epoch =
      deserialized_content.shared_member_key().rbegin()->epoch();
  if (last_shared_key_epoch == 0 && !state_.constant_key_allowed) {
    // Request without epoch/epoch set to zero is allowed iff constant key is
    // the only key.
    return CreateErrorResponse(net::HTTP_BAD_REQUEST);
  }
  if (last_shared_key_epoch != 0 &&
      state_.current_epoch != last_shared_key_epoch) {
    // Wrong epoch.
    return CreateErrorResponse(net::HTTP_BAD_REQUEST);
  }

  if (!VerifySharedKeys(deserialized_content, state_.trusted_vault_keys)) {
    return CreateErrorResponse(net::HTTP_BAD_REQUEST);
  }

  if (state_.current_epoch == 0) {
    // Simulate generation of random epoch when security domain just created.
    DCHECK(state_.public_key_to_shared_keys.empty());
    state_.current_epoch = 100;
  }

  state_.public_key_to_shared_keys[member.public_key()] =
      std::vector<trusted_vault_pb::SharedMemberKey>(
          deserialized_content.shared_member_key().begin(),
          deserialized_content.shared_member_key().end());
  return CreateHttpResponseForSuccessfulJoinSecurityDomainsRequest(
      state_.current_epoch);
}

std::unique_ptr<net::test_server::HttpResponse>
FakeSecurityDomainsServer::HandleGetSecurityDomainMemberRequest(
    const net::test_server::HttpRequest& http_request) {
  base::AutoLock autolock(lock_);
  GURL url = http_request.GetURL();
  const std::string member_public_key =
      GetPublicKeyFromGetSecurityDomainMemberRequestURL(http_request.GetURL(),
                                                        server_url_);
  if (member_public_key.empty()) {
    DVLOG(1) << "Member public key isn't presented or corrupted in "
                "GetSecurityDomainMemberRequest";
    return CreateErrorResponse(net::HTTP_BAD_REQUEST);
  }
  if (state_.public_key_to_shared_keys.count(member_public_key) == 0) {
    DVLOG(1) << "Member requested in GetSecurityDomainMemberRequest not found";
    return CreateErrorResponse(net::HTTP_NOT_FOUND);
  }
  trusted_vault_pb::SecurityDomainMember member;

  std::string encoded_public_key;
  base::Base64UrlEncode(member_public_key,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);

  member.set_name(kSecurityDomainMemberNamePrefix + encoded_public_key);
  member.set_public_key(member_public_key);

  trusted_vault_pb::SecurityDomainMember::SecurityDomainMembership* membership =
      member.add_memberships();
  membership->set_security_domain(
      GetSecurityDomainPath(SecurityDomainId::kChromeSync));
  for (const trusted_vault_pb::SharedMemberKey& shared_key :
       state_.public_key_to_shared_keys[member_public_key]) {
    *membership->add_keys() = shared_key;
  }
  for (const trusted_vault_pb::RotationProof& rotation_proof :
       state_.public_key_to_rotation_proofs[member_public_key]) {
    *membership->add_rotation_proofs() = rotation_proof;
  }

  member.set_member_type(
      trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE);
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(member.SerializeAsString());
  return response;
}

std::unique_ptr<net::test_server::HttpResponse>
FakeSecurityDomainsServer::HandleGetSecurityDomainRequest(
    const net::test_server::HttpRequest& http_request) {
  trusted_vault_pb::SecurityDomain security_domain;
  security_domain.mutable_security_domain_details()
      ->mutable_sync_details()
      ->set_degraded_recoverability(IsRecoverabilityDegraded());

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(security_domain.SerializeAsString());
  return response;
}

}  // namespace trusted_vault
