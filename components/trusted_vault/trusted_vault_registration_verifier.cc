// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_registration_verifier.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "components/signin/public/base/consent_level.h"
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/download_keys_response_handler.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_impl.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_request.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace trusted_vault {
namespace {

TrustedVaultDownloadKeysStatusForUMA GetDownloadKeysStatusForUMAFromResponse(
    TrustedVaultDownloadKeysStatus response_status) {
  switch (response_status) {
    case TrustedVaultDownloadKeysStatus::kSuccess:
      return TrustedVaultDownloadKeysStatusForUMA::kSuccess;
    case TrustedVaultDownloadKeysStatus::kMemberNotFound:
      return TrustedVaultDownloadKeysStatusForUMA::kMemberNotFound;
    case TrustedVaultDownloadKeysStatus::kMembershipNotFound:
      return TrustedVaultDownloadKeysStatusForUMA::kMembershipNotFound;
    case TrustedVaultDownloadKeysStatus::kMembershipCorrupted:
      return TrustedVaultDownloadKeysStatusForUMA::kMembershipCorrupted;
    case TrustedVaultDownloadKeysStatus::kMembershipEmpty:
      return TrustedVaultDownloadKeysStatusForUMA::kMembershipEmpty;
    case TrustedVaultDownloadKeysStatus::kNoNewKeys:
      return TrustedVaultDownloadKeysStatusForUMA::kNoNewKeys;
    case TrustedVaultDownloadKeysStatus::kKeyProofsVerificationFailed:
      return TrustedVaultDownloadKeysStatusForUMA::kKeyProofsVerificationFailed;
    case TrustedVaultDownloadKeysStatus::kAccessTokenFetchingFailure:
      return TrustedVaultDownloadKeysStatusForUMA::kAccessTokenFetchingFailure;
    case TrustedVaultDownloadKeysStatus::kNetworkError:
      return TrustedVaultDownloadKeysStatusForUMA::kNetworkError;
    case TrustedVaultDownloadKeysStatus::kOtherError:
      return TrustedVaultDownloadKeysStatusForUMA::kOtherError;
  }

  NOTREACHED();
  return TrustedVaultDownloadKeysStatusForUMA::kOtherError;
}

}  // namespace

TrustedVaultRegistrationVerifier::TrustedVaultRegistrationVerifier(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      access_token_fetcher_frontend_(identity_manager),
      access_token_fetcher_(access_token_fetcher_frontend_.GetWeakPtr()) {
  DCHECK(identity_manager_);
}

TrustedVaultRegistrationVerifier::~TrustedVaultRegistrationVerifier() = default;

void TrustedVaultRegistrationVerifier::VerifyMembership(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key) {
  DCHECK(!gaia_id.empty());
  CoreAccountInfo primary_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account.gaia != gaia_id) {
    // Ignore the call, as verification is only interesting for the primary
    // account.
    return;
  }

  const GURL trusted_vault_service_url =
      ExtractTrustedVaultServiceURLFromCommandLine();

  auto request = std::make_unique<TrustedVaultRequest>(
      primary_account.account_id, TrustedVaultRequest::HttpMethod::kGet,
      GURL(trusted_vault_service_url.spec() +
           GetGetSecurityDomainMemberURLPathAndQuery(public_key)),
      /*serialized_request_proto=*/absl::nullopt,
      /*max_retry_duration=*/base::Seconds(0), url_loader_factory_,
      access_token_fetcher_.Clone(),
      TrustedVaultURLFetchReasonForUMA::kDownloadKeys);

  request->FetchAccessTokenAndSendRequest(
      base::BindOnce([](TrustedVaultRequest::HttpStatus http_status,
                        const std::string& response_body) {
        absl::optional<TrustedVaultDownloadKeysStatus> status =
            DownloadKeysResponseHandler::GetErrorFromHttpStatus(http_status);
        // This function is only invoked for V1 registrations. The code below
        // also uses kKeyProofsVerificationFailed semi-arbitrarily to represent
        // that verification wasn't possible (since the private key wasn't
        // known).
        RecordVerifyRegistrationStatus(
            GetDownloadKeysStatusForUMAFromResponse(status.value_or(
                TrustedVaultDownloadKeysStatus::kKeyProofsVerificationFailed)),
            /*also_log_with_v1_suffix=*/true);
      }));

  ongoing_verify_registration_request_ = std::move(request);
}

}  // namespace trusted_vault
