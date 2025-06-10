// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/recovery_key_store_connection_impl.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/recovery_key_store_certificate.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_request.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace trusted_vault {

namespace {

// The "/0" suffix is required but ignored.
constexpr char kUpdateVaultUrl[] =
    "https://cryptauthvault.googleapis.com/v1/vaults/0";

constexpr char kListVaultsUrl[] =
    "https://cryptauthvault.googleapis.com/v1/"
    "vaults?use_case=13&challenge_not_required=1";

constexpr char kRecoveryKeyStoreCertFileUrl[] =
    "https://www.gstatic.com/cryptauthvault/v0/cert.xml";

constexpr char kRecoveryKeyStoreSigFileUrl[] =
    "https://www.gstatic.com/cryptauthvault/v0/cert.sig.xml";

// The maximum number of bytes that will be downloaded from the above two URLs.
constexpr size_t kMaxRecoveryKeyStoreCertFetchBodyBytes = 128 * 1024;

static constexpr net::NetworkTrafficAnnotationTag kCertXmlTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("trusted_vault_cert_request",
                                        R"(
      semantics {
        sender: "Trusted Vault Service"
        description:
          "Request to the vault service in order to retrieve the public list "
          "of certificates to perform encryption key operations for Chrome, "
          "such as Google Password Manager passkey operations"
        trigger:
          "Periodically/upon certain non-user controlled events after user "
          "signs in Chrome profile."
        data: "None"
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2025-06-05"
        user_data {
          type: NONE
        }
        internal {
          contacts {
            email: "nsatragno@chromium.org"
          }
          contacts {
            email: "chrome-webauthn@google.com"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature cannot be disabled in settings, but if user signs "
          "out of Chrome, this request would not be made."
        chrome_policy {
          SigninAllowed {
            policy_options {mode: MANDATORY}
            SigninAllowed: false
          }
        }
      })");

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

// Represents a request to fetch the recovery key store certificates.
class FetchRecoveryKeyStoreCertificatesRequest
    : public TrustedVaultConnection::Request {
 public:
  // `clock` must outlive this.
  FetchRecoveryKeyStoreCertificatesRequest(
      base::Clock* clock,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      RecoveryKeyStoreConnection::FetchRecoveryKeyStoreCertificatesCallback
          callback)
      : clock_(clock),
        url_loader_factory_(std::move(url_loader_factory)),
        callback_(std::move(callback)) {
    // Destroying `this` will also destroy the URL loader, which means these
    // callbacks will never complete. Thus, unretained is fine.
    file_download_closure_ = base::BarrierClosure(
        2, base::BindOnce(
               &FetchRecoveryKeyStoreCertificatesRequest::OnDownloadComplete,
               base::Unretained(this)));
    cert_xml_loader_ =
        Fetch(kRecoveryKeyStoreCertFileUrl,
              base::BindOnce(
                  &FetchRecoveryKeyStoreCertificatesRequest::OnFileReceived,
                  base::Unretained(this), &cert_xml_));
    sig_xml_loader_ =
        Fetch(kRecoveryKeyStoreSigFileUrl,
              base::BindOnce(
                  &FetchRecoveryKeyStoreCertificatesRequest::OnFileReceived,
                  base::Unretained(this), &sig_xml_));
  }

 private:
  // Fetches `url` and runs `callback` when done.
  std::unique_ptr<network::SimpleURLLoader> Fetch(
      const std::string_view url,
      base::OnceCallback<void(std::optional<std::string>)> callback) {
    auto network_request = std::make_unique<network::ResourceRequest>();
    GURL request_url(url);
    CHECK(request_url.is_valid());
    network_request->url = std::move(request_url);

    auto loader = network::SimpleURLLoader::Create(std::move(network_request),
                                                   kCertXmlTrafficAnnotation);
    loader->SetTimeoutDuration(base::Seconds(10));
    loader->SetURLLoaderFactoryOptions(
        network::mojom::kURLLoadOptionBlockAllCookies);
    loader->DownloadToString(url_loader_factory_.get(), std::move(callback),
                             kMaxRecoveryKeyStoreCertFetchBodyBytes);
    return loader;
  }

  // Called after a fetch request finishes.
  void OnFileReceived(std::string* destination,
                      std::optional<std::string> file) {
    if (!file) {
      FinishRequestAndMaybeDestroySelf(base::unexpected(
          RecoveryKeyStoreCertificateFetchStatus::kNetworkError));
      return;
    }
    *destination = std::move(*file);
    file_download_closure_.Run();
  }

  // Called after both `sig_xml_` and `cert_xml_` have been downloaded.
  void OnDownloadComplete() {
    std::optional<RecoveryKeyStoreCertificate> certificate =
        RecoveryKeyStoreCertificate::Parse(cert_xml_, sig_xml_, clock_->Now());
    if (certificate) {
      FinishRequestAndMaybeDestroySelf(std::move(*certificate));
      return;
    }
    FinishRequestAndMaybeDestroySelf(
        base::unexpected(RecoveryKeyStoreCertificateFetchStatus::kParseError));
  }

  void FinishRequestAndMaybeDestroySelf(
      base::expected<RecoveryKeyStoreCertificate,
                     RecoveryKeyStoreCertificateFetchStatus> result) {
    RecoveryKeyStoreCertificatesFetchStatusForUMA status =
        RecoveryKeyStoreCertificatesFetchStatusForUMA::kSuccess;
    if (!result.has_value()) {
      switch (result.error()) {
        case RecoveryKeyStoreCertificateFetchStatus::kNetworkError:
          status = RecoveryKeyStoreCertificatesFetchStatusForUMA::kNetworkError;
          break;
        case RecoveryKeyStoreCertificateFetchStatus::kParseError:
          status = RecoveryKeyStoreCertificatesFetchStatusForUMA::kParseError;
          break;
        case RecoveryKeyStoreCertificateFetchStatus::kSuccess:
          NOTREACHED();
      }
    }
    RecordRecoveryKeyStoreFetchCertificatesStatus(status);
    std::move(callback_).Run(std::move(result));
  }

  raw_ptr<base::Clock> clock_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  RecoveryKeyStoreConnection::FetchRecoveryKeyStoreCertificatesCallback
      callback_;
  base::RepeatingClosure file_download_closure_;
  std::unique_ptr<network::SimpleURLLoader> cert_xml_loader_;
  std::unique_ptr<network::SimpleURLLoader> sig_xml_loader_;
  std::string cert_xml_;
  std::string sig_xml_;
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

std::unique_ptr<RecoveryKeyStoreConnectionImpl::Request>
RecoveryKeyStoreConnectionImpl::FetchRecoveryKeyStoreCertificates(
    base::Clock* clock,
    FetchRecoveryKeyStoreCertificatesCallback callback) {
  return std::make_unique<FetchRecoveryKeyStoreCertificatesRequest>(
      clock, URLLoaderFactory(), std::move(callback));
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
