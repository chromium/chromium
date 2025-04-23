// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_FAKE_SECURITY_DOMAIN_SERVICE_H_
#define CHROME_BROWSER_WEBAUTHN_FAKE_SECURITY_DOMAIN_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "net/http/http_status_code.h"

namespace network {
struct ResourceRequest;
}

namespace trusted_vault_pb {
class SecurityDomainMember;
}

// A fake implementation of the Security Domain Service (SDS) for passkeys
// testing. This implementation can be fed network requests via the callback
// returned by `GetCallback`. If the request URL is for the SDS then it'll
// return an HTTP status and response body.
//
// This implementation will record members and enforce that the correct epochs
// are set in requests. It assumes that only a single security domain is being
// used.
class FakeSecurityDomainService {
 public:
  // If present, values of this type contain an HTTP status code (e.g. 200) and
  // the body of the response.
  using MaybeResponse =
      std::optional<std::pair<net::HttpStatusCode, std::string>>;

  using JoinMemberMatcher = base::RepeatingCallback<bool(
      const trusted_vault_pb::JoinSecurityDomainsRequest&)>;

  static std::unique_ptr<FakeSecurityDomainService> New(int epoch);

  virtual ~FakeSecurityDomainService() = 0;

  // Get a callback that processes network requests and, if they are for the
  // security domain service, returns a response.
  virtual base::RepeatingCallback<
      MaybeResponse(const network::ResourceRequest&)>
  GetCallback() = 0;

  // If called, all future requests will return HTTP 500 errors.
  virtual void fail_all_requests() = 0;

  // If called, the security domain will accept a join request with the correct
  // epoch, as if MagicArch had just completed.
  virtual void pretend_there_are_members() = 0;

  // Returns an HTTP 500 when the request matches |filter|.
  virtual void fail_join_requests_matching(JoinMemberMatcher filter) = 0;

  // Simulates the user resetting the security domain.
  virtual void ResetSecurityDomain() = 0;

  // Changes the GPM PIN recovery member to not be usable, i.e.
  // usable_for_recovery is set to false and no metadata is returned.
  virtual void MakePinMemberUnusable() = 0;

  // Removes the GPM PIN recovery member.
  virtual void RemovePinMember() = 0;

  // Updates the public key of the PIN member.
  virtual void SetPinMemberPublicKey(std::string public_key) = 0;

  virtual size_t num_physical_members() const = 0;
  virtual size_t num_pin_members() const = 0;
  virtual std::string GetPinMemberPublicKey() const = 0;
  virtual trusted_vault::GpmPinMetadata GetPinMetadata() const = 0;
  virtual base::span<const trusted_vault_pb::SecurityDomainMember> members()
      const = 0;
};

#endif  // CHROME_BROWSER_WEBAUTHN_FAKE_SECURITY_DOMAIN_SERVICE_H_
