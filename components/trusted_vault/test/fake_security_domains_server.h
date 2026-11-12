// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TEST_FAKE_SECURITY_DOMAINS_SERVER_H_
#define COMPONENTS_TRUSTED_VAULT_TEST_FAKE_SECURITY_DOMAINS_SERVER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace trusted_vault {

class SecureBoxKeyPair;

// Mimics behavior of the security domains server. This class is designed to be
// used with EmbeddedTestServer via registration of HandleRequest() method.
class FakeSecurityDomainsServer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when handling of request is completed. Called iff request is
    // supposed to be handled by FakeSecurityDomainsServer (determined by the
    // URL prefix).
    virtual void OnRequestHandled() = 0;
  };

  enum class DomainStatus {
    // No members have joined and no keys have been set.
    kEmpty,
    // Only the constant pre-enrollment key is present in the security domain.
    kPreEnrolled,
    // At least one non-constant key is present in the security domain.
    kEnrolled,
  };

  // Represents a member registered in the security domain.
  struct Member {
    std::vector<uint8_t> public_key;
    trusted_vault_pb::SecurityDomainMember::MemberType member_type;

    friend bool operator==(const Member&, const Member&) = default;
  };

  // Returns whether recoverability of the security domain is degraded given
  // its current `status` and registered `members`.
  using IsRecoverabilityDegradedCallback =
      base::RepeatingCallback<bool(DomainStatus status,
                                   base::span<const Member> members)>;

  static GURL GetServerURL(GURL base_url);

  explicit FakeSecurityDomainsServer(GURL base_url);
  FakeSecurityDomainsServer(const FakeSecurityDomainsServer& other) = delete;
  FakeSecurityDomainsServer& operator=(const FakeSecurityDomainsServer& other) =
      delete;
  ~FakeSecurityDomainsServer();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Handles request if it belongs to security domains server (identified by
  // request url). Returns nullptr otherwise.
  // Unlike other methods of this class, which should be called on main thread,
  // this method is called on EmbeddedTestServer IO thread.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& http_request);

  // Rotates trusted vault key by adding new shared key for all members. Returns
  // new trusted vault key.
  std::vector<uint8_t> RotateTrustedVaultKey(
      const std::vector<uint8_t>& last_trusted_vault_key);

  // Resets |state_| to the initial one.
  void ResetData();

  // Resets |state_| to the one derived from parameters.
  void ResetDataToState(const std::vector<std::vector<uint8_t>>& keys,
                        int last_key_version);

  // Sets a callback evaluated by `IsRecoverabilityDegraded()` against the
  // security domain's current status and members. If unset (or null),
  // recoverability is not degraded. `callback` is invoked while `lock_` is
  // held.
  void SetIsRecoverabilityDegradedCallback(
      IsRecoverabilityDegradedCallback callback);

  // Sets (or clears if null) the Gaia password encryption key pair returned by
  // `GetCurrentGaiaPasswordEncryptionKeyData` and used to unwrap keys during
  // key rotation.
  void SetGaiaPasswordKeyPair(
      std::unique_ptr<SecureBoxKeyPair> gaia_password_key_pair);

  // Returns the number of members registered in the security domain.
  int GetMemberCount() const;

  // Returns all members registered in the security domain.
  std::vector<Member> GetMembers() const;

  // Returns all trusted vault keys in the security domain, ordered by
  // increasing epoch.
  std::vector<std::vector<uint8_t>> GetAllTrustedVaultKeys() const;

  // Returns whether every member in the security domain holds a shared key
  // with a valid proof for `trusted_vault_key`.
  bool AllMembersHaveKey(const std::vector<uint8_t>& trusted_vault_key) const;

  // Returns the current epoch of the security domain (0 when no members have
  // joined and no keys have been set).
  int GetCurrentEpoch() const;

  // Returns whether recoverability of the security domain is currently
  // degraded according to the callback set via
  // `SetIsRecoverabilityDegradedCallback()`.
  bool IsRecoverabilityDegraded() const;

  // Returns true if there was a request that violates supported protocol.
  bool ReceivedInvalidRequest() const;

 private:
  std::vector<Member> GetMembersLocked() const EXCLUSIVE_LOCKS_REQUIRED(lock_);

  std::unique_ptr<net::test_server::HttpResponse>
  HandleJoinSecurityDomainsRequest(
      const net::test_server::HttpRequest& http_request);

  std::unique_ptr<net::test_server::HttpResponse>
  HandleListSecurityDomainMembersRequest(
      const net::test_server::HttpRequest& http_request);

  std::unique_ptr<net::test_server::HttpResponse>
  HandleGetSecurityDomainMemberRequest(
      const net::test_server::HttpRequest& http_request);

  std::unique_ptr<net::test_server::HttpResponse>
  HandleGetSecurityDomainRequest(
      const net::test_server::HttpRequest& http_request);

  std::unique_ptr<net::test_server::HttpResponse>
  HandleDownloadGaiaPasswordPublicKeyRequest(
      const net::test_server::HttpRequest& http_request);

  std::unique_ptr<net::test_server::HttpResponse> HandleRotateSharedKeyRequest(
      const net::test_server::HttpRequest& http_request);

  class State {
   public:
    State();
    State(const State& other) = delete;
    State(State&& other);
    State& operator=(const State& other) = delete;
    State& operator=(State&& other);
    ~State();

    bool received_invalid_request = false;

    // Maps members public key to shared keys that belong to this member.
    std::map<std::string, std::vector<trusted_vault_pb::SharedMemberKey>>
        public_key_to_shared_keys;
    // Maps members public key to member type.
    std::map<std::string, trusted_vault_pb::SecurityDomainMember::MemberType>
        public_key_to_member_type;
    // Maps members public key to rotation proofs of members shared keys.
    std::map<std::string, std::vector<trusted_vault_pb::RotationProof>>
        public_key_to_rotation_proofs;

    // Zero epoch is used when there are no members in the security domain, once
    // first member is joined it is initialized to non-zero value. Members still
    // can join with constant key without populating epoch while |status| is not
    // `DomainStatus::kEnrolled`.
    int current_epoch = 0;
    DomainStatus status = DomainStatus::kEmpty;

    // All trusted vault keys ordered by increasing epoch.
    std::vector<std::vector<uint8_t>> trusted_vault_keys;

    IsRecoverabilityDegradedCallback is_recoverability_degraded_callback;

    std::unique_ptr<SecureBoxKeyPair> gaia_password_key_pair;
  };

  // This class is used on main thread and on EmbeddedTestServer IO thread, data
  // access is protected by |lock_|.
  mutable base::Lock lock_;

  const GURL server_url_;
  State state_ GUARDED_BY(lock_);

  const scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TEST_FAKE_SECURITY_DOMAINS_SERVER_H_
