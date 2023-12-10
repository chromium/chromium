// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_IMPL_H_

#include <memory>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace password_manager {

class BulkLeakCheckDelegateInterface;
struct SingleLookupResponse;

// Implementation of the bulk leak check.
// Every credential in the list is processed consequitively:
// - prepare payload for the request.
// - get the access token.
// - make a network request.
// - decrypt the response.
// Encryption/decryption part is expensive and, therefore, done only on one
// background sequence.
class BulkLeakCheckImpl : public BulkLeakCheck {
 public:
  struct CredentialHolder;

  BulkLeakCheckImpl(
      BulkLeakCheckDelegateInterface* delegate,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~BulkLeakCheckImpl() override;

  // BulkLeakCheck:
  void CheckCredentials(LeakDetectionInitiator initiator,
                        std::vector<LeakCheckCredential> credentials) override;
  size_t GetPendingChecksCount() const override;

#if defined(UNIT_TEST)
  void set_network_factory(
      std::unique_ptr<LeakDetectionRequestFactory> factory) {
    network_request_factory_ = std::move(factory);
  }
#endif  // defined(UNIT_TEST)

 private:
  // Called when a payload for one credential is ready.
  void OnPayloadReady(CredentialHolder* weak_holder,
                      LookupSingleLeakPayload payload);

  // Called when an access token is ready for |weak_holder|.
  void OnTokenReady(CredentialHolder* weak_holder,
                    GoogleServiceAuthError error,
                    signin::AccessTokenInfo access_token_info);

  // Called when the server replied with something.
  void OnLookupLeakResponse(CredentialHolder* weak_holder,
                            std::unique_ptr<SingleLookupResponse> response,
                            std::optional<LeakDetectionError> error);

  // Called when the response was analyzed on the background thread.
  void OnAnalyzedResponse(CredentialHolder* weak_holder,
                          AnalyzeResponseResult result);

  // Delegate for the instance. Should outlive |this|.
  const raw_ptr<BulkLeakCheckDelegateInterface> delegate_;

  // Identity manager for the profile.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // URL loader factory required for the network request to the identity
  // endpoint.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // A factory for creating network requests.
  std::unique_ptr<LeakDetectionRequestFactory> network_request_factory_ =
      std::make_unique<LeakDetectionRequestFactory>();

  // The key used to encrypt/decrypt all the payloads for all the credentials.
  // Creating it once saves CPU time.
  const std::string encryption_key_;

  // Every LeakCheckCredential is moved from one state to the next one in the
  // following order.
  // Hash/encrypt the payload -> request an access token -> make a network
  // request -> process the payload.
  // Below are the queues for each state.

  // The queue of the requests waiting for the payload compilation happening on
  // the background thread. When the payload is ready, the element is moved to
  // |waiting_token_| queue.
  base::circular_deque<std::unique_ptr<CredentialHolder>> waiting_encryption_;

  // The queue of the requests waiting for the payload compilation happening on
  // the background thread.
  base::circular_deque<std::unique_ptr<CredentialHolder>> waiting_token_;

  // The queue of the requests waiting for the server reply.
  base::circular_deque<std::unique_ptr<CredentialHolder>> waiting_response_;

  // The queue of the requests waiting for server response decoding.
  base::circular_deque<std::unique_ptr<CredentialHolder>> waiting_decryption_;

  // Task runner for preparing the payload. It takes a lot of memory. Therefore,
  // those tasks aren't parallelized.
  scoped_refptr<base::SequencedTaskRunner> payload_task_runner_;

  // Cancels pending encryption tasks when destructing.
  base::CancelableTaskTracker task_tracker_;

  // Weak pointers for different callbacks.
  base::WeakPtrFactory<BulkLeakCheckImpl> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_IMPL_H_
