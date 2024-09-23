// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_impl.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {
namespace {

using HolderPtr = std::unique_ptr<BulkLeakCheckImpl::CredentialHolder>;
HolderPtr RemoveFromQueue(BulkLeakCheckImpl::CredentialHolder* weak_holder,
                          base::circular_deque<HolderPtr>* queue) {
  auto it = base::ranges::find(*queue, weak_holder, &HolderPtr::get);
  CHECK(it != queue->end());
  HolderPtr holder = std::move(*it);
  queue->erase(it);
  return holder;
}

}  // namespace

// Holds all necessary payload for the request to the server for one credential.
struct BulkLeakCheckImpl::CredentialHolder {
  explicit CredentialHolder(LeakCheckCredential c) : credential(std::move(c)) {}
  ~CredentialHolder() = default;

  CredentialHolder(const CredentialHolder&) = delete;
  CredentialHolder& operator=(const CredentialHolder&) = delete;

  // Client supplied credential to be checked.
  LeakCheckCredential credential;

  // Payload to be sent to the server.
  LookupSingleLeakPayload payload;

  // Request for the needed access token.
  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher;

  // Network request for the API call.
  std::unique_ptr<LeakDetectionRequestInterface> network_request_;
};

LeakCheckCredential::LeakCheckCredential(std::u16string username,
                                         std::u16string password)
    : username_(std::move(username)), password_(std::move(password)) {}

LeakCheckCredential::LeakCheckCredential(LeakCheckCredential&&) = default;

LeakCheckCredential& LeakCheckCredential::operator=(LeakCheckCredential&&) =
    default;

LeakCheckCredential::~LeakCheckCredential() = default;

BulkLeakCheckImpl::BulkLeakCheckImpl(
    BulkLeakCheckDelegateInterface* delegate,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : delegate_(delegate),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      encryption_key_(CreateNewKey().value_or("")),
      payload_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  DCHECK(delegate_);
  DCHECK(identity_manager_);
  DCHECK(url_loader_factory_);
  DCHECK(!encryption_key_.empty());
}

BulkLeakCheckImpl::~BulkLeakCheckImpl() = default;

void BulkLeakCheckImpl::CheckCredentials(
    LeakDetectionInitiator initiator,
    std::vector<LeakCheckCredential> credentials) {
  for (auto& c : credentials) {
    waiting_encryption_.push_back(
        std::make_unique<CredentialHolder>(std::move(c)));
    const LeakCheckCredential& credential =
        waiting_encryption_.back()->credential;
    PrepareSingleLeakRequestData(
        task_tracker_, *payload_task_runner_, initiator, encryption_key_,
        base::UTF16ToUTF8(credential.username()),
        base::UTF16ToUTF8(credential.password()),
        base::BindOnce(&BulkLeakCheckImpl::OnPayloadReady,
                       weak_ptr_factory_.GetWeakPtr(),
                       waiting_encryption_.back().get()));
  }
}

size_t BulkLeakCheckImpl::GetPendingChecksCount() const {
  return waiting_encryption_.size() + waiting_token_.size() +
         waiting_response_.size() + waiting_decryption_.size();
}

void BulkLeakCheckImpl::OnPayloadReady(CredentialHolder* weak_holder,
                                       LookupSingleLeakPayload payload) {
  std::unique_ptr<CredentialHolder> holder =
      RemoveFromQueue(weak_holder, &waiting_encryption_);
  if (payload.encrypted_payload.empty() ||
      payload.username_hash_prefix.empty()) {
    delegate_->OnError(LeakDetectionError::kHashingFailure);
    // |this| can be destroyed here.
    return;
  }

  holder->payload = std::move(payload);
  holder->token_fetcher = RequestAccessToken(
      identity_manager_,
      base::BindOnce(&BulkLeakCheckImpl::OnTokenReady,
                     weak_ptr_factory_.GetWeakPtr(), holder.get()));
  DCHECK(holder->token_fetcher);
  waiting_token_.push_back(std::move(holder));
}

void BulkLeakCheckImpl::OnTokenReady(
    CredentialHolder* weak_holder,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  std::unique_ptr<CredentialHolder> holder =
      RemoveFromQueue(weak_holder, &waiting_token_);
  if (error.state() != GoogleServiceAuthError::NONE) {
    if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED) {
      delegate_->OnError(LeakDetectionError::kNetworkError);
    } else if (error.state() ==
               GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
      delegate_->OnError(LeakDetectionError::kNotSignIn);
    } else {
      delegate_->OnError(LeakDetectionError::kTokenRequestFailure);
    }
    // |this| can be destroyed here.
    return;
  }

  holder->token_fetcher.reset();
  holder->network_request_ = network_request_factory_->CreateNetworkRequest();
  holder->network_request_->LookupSingleLeak(
      url_loader_factory_.get(), access_token_info.token,
      /*api_key=*/std::nullopt, std::move(holder->payload),
      base::BindOnce(&BulkLeakCheckImpl::OnLookupLeakResponse,
                     weak_ptr_factory_.GetWeakPtr(), holder.get()));
  waiting_response_.push_back(std::move(holder));
}

void BulkLeakCheckImpl::OnLookupLeakResponse(
    CredentialHolder* weak_holder,
    std::unique_ptr<SingleLookupResponse> response,
    std::optional<LeakDetectionError> error) {
  std::unique_ptr<CredentialHolder> holder =
      RemoveFromQueue(weak_holder, &waiting_response_);

  holder->network_request_.reset();
  if (!response) {
    delegate_->OnError(*error);
    return;
  }

  AnalyzeResponse(std::move(response), encryption_key_,
                  base::BindOnce(&BulkLeakCheckImpl::OnAnalyzedResponse,
                                 weak_ptr_factory_.GetWeakPtr(), holder.get()));
  waiting_decryption_.push_back(std::move(holder));
}

void BulkLeakCheckImpl::OnAnalyzedResponse(CredentialHolder* weak_holder,
                                           AnalyzeResponseResult result) {
  std::unique_ptr<CredentialHolder> holder =
      RemoveFromQueue(weak_holder, &waiting_decryption_);
  switch (result) {
    case AnalyzeResponseResult::kDecryptionError:
      delegate_->OnError(LeakDetectionError::kHashingFailure);
      return;
    case AnalyzeResponseResult::kNotLeaked:
    case AnalyzeResponseResult::kLeaked:
      delegate_->OnFinishedCredential(
          std::move(holder->credential),
          IsLeaked(result == AnalyzeResponseResult::kLeaked));
      return;
  }
}

}  // namespace password_manager
