// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/recovery_key_store_connection_impl.h"

#include <memory>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_request.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace trusted_vault {
namespace {

// The "/0" suffix is required but ignored.
constexpr char kUpdateVaultUrl[] =
    "https://cryptauthvault.googleapis.com/v1/vaults/0";

constexpr char kListVaultsUrl[] =
    "https://cryptauthvault.googleapis.com/v1/"
    "vaults?use_case=13&challenge_not_required=1";

RecoveryKeyStoreStatus HttpStatusToRecoveryKeyStoreStatus(
    TrustedVaultRequest::HttpStatus http_status) {
  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
      return RecoveryKeyStoreStatus::kSuccess;
    case TrustedVaultRequest::HttpStatus::kBadRequest:
    case TrustedVaultRequest::HttpStatus::kNotFound:
    case TrustedVaultRequest::HttpStatus::kConflict:
    case TrustedVaultRequest::HttpStatus::kOtherError:
      return RecoveryKeyStoreStatus::kOtherError;
    case TrustedVaultRequest::HttpStatus::kNetworkError:
      return RecoveryKeyStoreStatus::kNetworkError;
    case TrustedVaultRequest::HttpStatus::kTransientAccessTokenFetchError:
      return RecoveryKeyStoreStatus::kTransientAccessTokenFetchError;
    case TrustedVaultRequest::HttpStatus::kPersistentAccessTokenFetchError:
      return RecoveryKeyStoreStatus::kPersistentAccessTokenFetchError;
    case TrustedVaultRequest::HttpStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
      return RecoveryKeyStoreStatus::kPrimaryAccountChangeAccessTokenFetchError;
  }
}

void ProcessUpdateVaultResponseResponse(
    RecoveryKeyStoreConnection::UpdateRecoveryKeyStoreCallback callback,
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  std::move(callback).Run(HttpStatusToRecoveryKeyStoreStatus(http_status));
}

// Represents a request to the listVaults RPC endpoint.
class ListRecoveryKeyStoresRequest : public TrustedVaultConnection::Request {
 public:
  ListRecoveryKeyStoresRequest(
      const CoreAccountInfo& account_info,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher,
      RecoveryKeyStoreConnection::ListRecoveryKeyStoresCallback callback)
      : account_id_(account_info.account_id),
        url_loader_factory_(std::move(url_loader_factory)),
        access_token_fetcher_(std::move(access_token_fetcher)),
        callback_(std::move(callback)) {
    StartOrContinueRequest();
  }

 private:
  void StartOrContinueRequest(const std::string* next_page_token = nullptr) {
    TrustedVaultRequest::RecordFetchStatusCallback record_fetch_status_to_uma =
        base::BindRepeating(
            &RecordRecoveryKeyStoreURLFetchResponse,
            RecoveryKeyStoreURLFetchReasonForUMA::kListRecoveryKeyStores);
    GURL request_url(kListVaultsUrl);
    request_ = std::make_unique<TrustedVaultRequest>(
        SecurityDomainId::kPasskeys, account_id_,
        TrustedVaultRequest::HttpMethod::kGet,
        next_page_token ? net::AppendQueryParameter(request_url, "page_token",
                                                    *next_page_token)
                        : request_url,
        /*serialized_request_proto=*/std::nullopt,
        /*max_retry_duration=*/base::Seconds(0), url_loader_factory_,
        access_token_fetcher_->Clone(), std::move(record_fetch_status_to_uma));

    // Unretained: this object owns `request_`. When `request_` is deleted, so
    // is the `SimpleURLLoader` and thus any callback is canceled.
    request_->FetchAccessTokenAndSendRequest(
        base::BindOnce(&ListRecoveryKeyStoresRequest::ProcessResponse,
                       base::Unretained(this)));
  }

  void ProcessResponse(TrustedVaultRequest::HttpStatus http_status,
                       const std::string& response_body) {
    if (http_status != TrustedVaultRequest::HttpStatus::kSuccess) {
      FinishWithResultAndMaybeDestroySelf(
          base::unexpected(HttpStatusToRecoveryKeyStoreStatus(http_status)));
      return;
    }
    trusted_vault_pb::ListVaultsResponse response;
    if (!response.ParseFromString(response_body)) {
      FinishWithResultAndMaybeDestroySelf(
          base::unexpected(RecoveryKeyStoreStatus::kOtherError));
      return;
    }
    for (const auto& vault : response.vaults()) {
      RecoveryKeyStoreEntry entry;
      entry.backend_public_key =
          ProtoStringToBytes(vault.vault_parameters().backend_public_key());
      entry.vault_handle =
          ProtoStringToBytes(vault.vault_parameters().vault_handle());
      result_.push_back(std::move(entry));
    }
    if (!response.next_page_token().empty()) {
      StartOrContinueRequest(&response.next_page_token());
      return;
    }
    FinishWithResultAndMaybeDestroySelf(std::move(result_));
  }

  void FinishWithResultAndMaybeDestroySelf(
      base::expected<std::vector<RecoveryKeyStoreEntry>, RecoveryKeyStoreStatus>
          result) {
    std::move(callback_).Run(std::move(result));
  }

  std::unique_ptr<TrustedVaultRequest> request_;
  const CoreAccountId account_id_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher_;
  RecoveryKeyStoreConnection::ListRecoveryKeyStoresCallback callback_;
  std::vector<RecoveryKeyStoreEntry> result_;
};

}  // namespace

RecoveryKeyStoreEntry::RecoveryKeyStoreEntry() = default;
RecoveryKeyStoreEntry::RecoveryKeyStoreEntry(RecoveryKeyStoreEntry&&) = default;
RecoveryKeyStoreEntry& RecoveryKeyStoreEntry::operator=(
    RecoveryKeyStoreEntry&&) = default;
RecoveryKeyStoreEntry::~RecoveryKeyStoreEntry() = default;
bool RecoveryKeyStoreEntry::operator==(const RecoveryKeyStoreEntry&) const =
    default;

RecoveryKeyStoreConnectionImpl::RecoveryKeyStoreConnectionImpl(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher)
    : pending_url_loader_factory_(std::move(pending_url_loader_factory)),
      access_token_fetcher_(std::move(access_token_fetcher)) {}

RecoveryKeyStoreConnectionImpl::~RecoveryKeyStoreConnectionImpl() = default;

std::unique_ptr<RecoveryKeyStoreConnectionImpl::Request>
RecoveryKeyStoreConnectionImpl::UpdateRecoveryKeyStore(
    const CoreAccountInfo& account_info,
    const trusted_vault_pb::Vault& vault,
    UpdateRecoveryKeyStoreCallback callback) {
  TrustedVaultRequest::RecordFetchStatusCallback record_fetch_status_to_uma =
      base::BindRepeating(
          &RecordRecoveryKeyStoreURLFetchResponse,
          RecoveryKeyStoreURLFetchReasonForUMA::kUpdateRecoveryKeyStore);
  auto request = std::make_unique<TrustedVaultRequest>(
      SecurityDomainId::kPasskeys, account_info.account_id,
      TrustedVaultRequest::HttpMethod::kPatch, GURL(kUpdateVaultUrl),
      vault.SerializeAsString(),
      /*max_retry_duration=*/base::Seconds(0), URLLoaderFactory(),
      access_token_fetcher_->Clone(), std::move(record_fetch_status_to_uma));
  request->FetchAccessTokenAndSendRequest(
      base::BindOnce(&ProcessUpdateVaultResponseResponse, std::move(callback)));
  return request;
}

std::unique_ptr<RecoveryKeyStoreConnectionImpl::Request>
RecoveryKeyStoreConnectionImpl::ListRecoveryKeyStores(
    const CoreAccountInfo& account_info,
    ListRecoveryKeyStoresCallback callback) {
  return std::make_unique<ListRecoveryKeyStoresRequest>(
      account_info, URLLoaderFactory(), access_token_fetcher_->Clone(),
      std::move(callback));
}

scoped_refptr<network::SharedURLLoaderFactory>
RecoveryKeyStoreConnectionImpl::URLLoaderFactory() {
  // `url_loader_factory_` is created lazily, because it needs to be done on
  // the backend sequence, while this class ctor is called on UI thread.
  if (!url_loader_factory_) {
    CHECK(pending_url_loader_factory_);
    url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(pending_url_loader_factory_));
  }
  return url_loader_factory_;
}

}  // namespace trusted_vault
