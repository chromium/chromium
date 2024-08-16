// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/fake_security_domain_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"

namespace {

constexpr char kSecurityDomainName[] = "users/me/securitydomains/hw_protected";

// Get the body of a request, assuming that it simply has a bytestring body.
// This is true of requests from the trusted_vault code.
std::string_view GetBody(const network::ResourceRequest& request) {
  const std::vector<network::DataElement>* elements =
      request.request_body->elements();
  CHECK_EQ(elements->size(), 1u);
  CHECK_EQ(elements->at(0).type(), network::DataElement::Tag::kBytes);
  return elements->at(0).As<network::DataElementBytes>().AsStringPiece();
}

class FakeSecurityDomainServiceImpl : public FakeSecurityDomainService {
 public:
  explicit FakeSecurityDomainServiceImpl(int epoch) : epoch_(epoch) {}

  ~FakeSecurityDomainServiceImpl() override = default;

  base::RepeatingCallback<MaybeResponse(const network::ResourceRequest&)>
  GetCallback() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return base::BindRepeating(
        [](base::WeakPtr<FakeSecurityDomainServiceImpl> impl,
           const network::ResourceRequest& request) -> MaybeResponse {
          // Passing a WeakPtr into BindRepeating is only supported if the
          // function returns void. Thus the WeakPtr is handled directly here.
          if (!impl) {
            return std::nullopt;
          }
          return impl->OnRequest(request);
        },
        weak_ptr_factory_.GetWeakPtr());
  }

  void fail_all_requests() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    fail_all_requests_ = true;
  }

  void pretend_there_are_members() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    pretend_there_are_members_ = true;
  }

  size_t num_physical_members() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return base::ranges::count_if(members_, [](const auto& member) -> bool {
      return member.member_type() == trusted_vault_pb::SecurityDomainMember::
                                         MEMBER_TYPE_PHYSICAL_DEVICE;
    });
  }

  size_t num_pin_members() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return base::ranges::count_if(members_, [](const auto& member) -> bool {
      return member.member_type() ==
             trusted_vault_pb::SecurityDomainMember::
                 MEMBER_TYPE_GOOGLE_PASSWORD_MANAGER_PIN;
    });
  }

  base::span<const trusted_vault_pb::SecurityDomainMember> members()
      const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return members_;
  }

 private:
  MaybeResponse OnRequest(const network::ResourceRequest& request) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!request.url.has_host() || !request.url.has_path() ||
        request.url.host_piece() != "securitydomain-pa.googleapis.com") {
      return std::nullopt;
    }

    if (fail_all_requests_) {
      return std::make_pair(net::HTTP_INTERNAL_SERVER_ERROR,
                            std::string("fail_all_requests() has been called"));
    }

    const std::string_view path = request.url.path_piece();
    if (path.starts_with("/v1/users/me/members")) {
      return ListMembers();
    } else if (path.starts_with("/v1/users/me/securitydomains/")) {
      return AddMember(request);
    } else {
      CHECK(false) << "Unhandled security domain service path: " << path;
      NOTREACHED();
    }
  }

  MaybeResponse ListMembers() {
    // We can't list a pretend member.
    CHECK(!pretend_there_are_members_);

    trusted_vault_pb::ListSecurityDomainMembersResponse response;
    for (const auto& member : members_) {
      auto* proto_member = response.add_security_domain_members();
      proto_member->CopyFrom(member);
    }

    return std::make_pair(net::HTTP_OK, response.SerializeAsString());
  }

  MaybeResponse AddMember(const network::ResourceRequest& request) {
    trusted_vault_pb::JoinSecurityDomainsRequest proto_request;
    CHECK(proto_request.ParseFromString(std::string(GetBody(request))));

    // Only the passkeys security domain is handled by this code.
    CHECK_EQ(proto_request.security_domain().name(), kSecurityDomainName);

    CHECK_EQ(proto_request.shared_member_key().size(), 1);

    // If the security domain is empty, the client must set a zero epoch.
    // Otherwise it must not.
    const int request_epoch = proto_request.shared_member_key()[0].epoch();
    CHECK_EQ(request_epoch == 0,
             !pretend_there_are_members_ && members_.empty());
    // If the client specified an epoch, it must be the correct one.
    CHECK(request_epoch == 0 || request_epoch == epoch_);

    if (proto_request.security_domain_member().member_type() ==
        trusted_vault_pb::SecurityDomainMember::
            MEMBER_TYPE_GOOGLE_PASSWORD_MANAGER_PIN) {
      CHECK(proto_request.security_domain_member().has_member_metadata());
      CHECK(proto_request.security_domain_member()
                .member_metadata()
                .has_google_password_manager_pin_metadata());
      CHECK(!proto_request.security_domain_member()
                 .member_metadata()
                 .google_password_manager_pin_metadata()
                 .encrypted_pin_hash()
                 .empty());

      const auto existing_pin =
          base::ranges::find_if(members_, [](const auto& member) -> bool {
            return member.member_type() ==
                   trusted_vault_pb::SecurityDomainMember::
                       MEMBER_TYPE_GOOGLE_PASSWORD_MANAGER_PIN;
          });
      if (existing_pin == members_.end()) {
        CHECK(proto_request.current_public_key_to_replace().empty());
      } else {
        CHECK(!proto_request.current_public_key_to_replace().empty());
        CHECK_EQ(proto_request.current_public_key_to_replace(),
                 existing_pin->public_key());
        members_.erase(existing_pin);
      }
    }

    auto* membership =
        proto_request.mutable_security_domain_member()->add_memberships();
    membership->set_security_domain(proto_request.security_domain().name());
    *membership->mutable_keys() = proto_request.shared_member_key();

    members_.push_back(proto_request.security_domain_member());

    trusted_vault_pb::JoinSecurityDomainsResponse response;
    auto* out_security_domain = response.mutable_security_domain();
    out_security_domain->set_name(kSecurityDomainName);
    out_security_domain->set_current_epoch(epoch_);

    // No need to pretend any longer.
    pretend_there_are_members_ = false;

    return std::make_pair(net::HTTP_OK, response.SerializeAsString());
  }

  const int epoch_;
  bool fail_all_requests_ = false;
  bool pretend_there_are_members_ = false;
  std::vector<trusted_vault_pb::SecurityDomainMember> members_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<FakeSecurityDomainServiceImpl> weak_ptr_factory_{this};
};

}  // namespace

FakeSecurityDomainService::~FakeSecurityDomainService() = default;

// static
std::unique_ptr<FakeSecurityDomainService> FakeSecurityDomainService::New(
    int epoch) {
  return std::make_unique<FakeSecurityDomainServiceImpl>(epoch);
}
