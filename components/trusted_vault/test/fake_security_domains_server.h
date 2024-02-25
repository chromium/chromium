// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TEST_FAKE_SECURITY_DOMAINS_SERVER_H_
#define COMPONENTS_TRUSTED_VAULT_TEST_FAKE_SECURITY_DOMAINS_SERVER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace trusted_vault {

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

  // Causes the security domain to enter the degraded recoverability state
  // unless the provided |public_key| matches a member's public key.
  void RequirePublicKeyToAvoidRecoverabilityDegraded(
      const std::vector<uint8_t>& public_key);

  int GetMemberCount() const;
  std::vector<std::vector<uint8_t>> GetAllTrustedVaultKeys() const;
  bool AllMembersHaveKey(const std::vector<uint8_t>& trusted_vault_key) const;
  int GetCurrentEpoch() const;
  bool IsRecoverabilityDegraded() const;

  // Returns true if there was a request that violates supported protocol.
  bool ReceivedInvalidRequest() const;

 private:
  std::unique_ptr<net::test_server::HttpResponse>
  HandleJoinSecurityDomainsRequest(
      const net::test_server::HttpRequest& http_request);

  std::unique_ptr<net::test_server::HttpResponse>
  HandleGetSecurityDomainMemberRequest(
      const net::test_server::HttpRequest& http_request);

  std::unique_ptr<net::test_server::HttpResponse>
  HandleGetSecurityDomainRequest(
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
    // Maps members public key to rotation proofs of members shared keys.
    std::map<std::string, std::vector<trusted_vault_pb::RotationProof>>
        public_key_to_rotation_proofs;

    // Zero epoch is used when there are no members in the security domain, once
    // first member is joined it is initialized to non-zero value. Members still
    // can join with constant key without populating epoch while
    // |constant_key_allowed_| set to true.
    int current_epoch = 0;
    bool constant_key_allowed = true;

    // All trusted vault keys ordered by increasing epoch.
    std::vector<std::vector<uint8_t>> trusted_vault_keys;

    std::string required_public_key_to_avoid_recoverability_degraded;
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
