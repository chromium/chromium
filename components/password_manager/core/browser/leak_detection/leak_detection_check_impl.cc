// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_check_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {
namespace {

using ::signin::AccessTokenFetcher;

// Wraps |callback| into another callback that measures the elapsed time between
// construction and actual execution of the callback. Records the result to
// |histogram|, which is expected to be a char literal.
template <typename R, typename... Args>
base::OnceCallback<R(Args...)> TimeCallback(
    base::OnceCallback<R(Args...)> callback,
    const char* histogram) {
  return base::BindOnce(
      [](const char* histogram, const base::ElapsedTimer& timer,
         base::OnceCallback<R(Args...)> callback, Args... args) {
        base::UmaHistogramTimes(histogram, timer.Elapsed());
        return std::move(callback).Run(std::forward<Args>(args)...);
      },
      histogram, base::ElapsedTimer(), std::move(callback));
}

}  // namespace

// Incapsulates the token request and payload calculation done in parallel.
class LeakDetectionCheckImpl::RequestPayloadHelper {
 public:
  RequestPayloadHelper(
      LeakDetectionCheckImpl* leak_check,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      absl::optional<std::string> api_key);
  ~RequestPayloadHelper() = default;

  // Neither copyable nor movable.
  RequestPayloadHelper(const RequestPayloadHelper&) = delete;
  RequestPayloadHelper& operator=(const RequestPayloadHelper&) = delete;
  RequestPayloadHelper(RequestPayloadHelper&&) = delete;
  RequestPayloadHelper& operator=(RequestPayloadHelper&&) = delete;

  // Returns |identity_manager_| for the profile.
  signin::IdentityManager* GetIdentityManager();

  void RequestAccessToken(AccessTokenFetcher::TokenCallback callback);
  void PreparePayload(const std::string& username,
                      const std::string& password,
                      SingleLeakRequestDataCallback callback);

  // Notifies that the access token was obtained.
  void OnGotAccessToken(absl::optional<std::string> access_token);

  // Notifies that the payload was obtained.
  void OnGotPayload(LookupSingleLeakData data);

 private:
  // Describes the steps done in parallel as a bit mask.
  enum Step {
    kPayloadData = 1 << 0,
    kAccessToken = 1 << 1,
    kAll = kPayloadData | kAccessToken
  };

  // If both the access token and the payload are ready notify |leak_check_|.
  void CheckAllStepsDone();

  // Bitmask of steps done.
  int steps_ = 0;
  // Owns |this|.
  raw_ptr<LeakDetectionCheckImpl> leak_check_;
  // Identity manager for the profile.
  raw_ptr<signin::IdentityManager> identity_manager_;
  // URL loader factory required for the network request to the identity
  // endpoint.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Actual request for the needed token.
  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher_;
  // The token to be used for request for signed-in user. It should be
  // |absl::nullopt| for signed-out users.
  absl::optional<std::string> access_token_;
  // Api key required to authenticate signed-out user. It should be
  // |absl::nullopt| for signed-in users.
  const absl::optional<std::string> api_key_;
  // Payload for the actual request.
  LookupSingleLeakData payload_;
};

LeakDetectionCheckImpl::RequestPayloadHelper::RequestPayloadHelper(
    LeakDetectionCheckImpl* leak_check,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    absl::optional<std::string> api_key)
    : leak_check_(leak_check),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      api_key_(std::move(api_key)) {
  DCHECK(identity_manager_);
  DCHECK(url_loader_factory_);
}

signin::IdentityManager*
LeakDetectionCheckImpl::RequestPayloadHelper::GetIdentityManager() {
  return identity_manager_;
}

void LeakDetectionCheckImpl::RequestPayloadHelper::RequestAccessToken(
    AccessTokenFetcher::TokenCallback callback) {
  token_fetcher_ = password_manager::RequestAccessToken(identity_manager_,
                                                        std::move(callback));
}

void LeakDetectionCheckImpl::RequestPayloadHelper::PreparePayload(
    const std::string& username,
    const std::string& password,
    SingleLeakRequestDataCallback callback) {
  PrepareSingleLeakRequestData(username, password, std::move(callback));
}

void LeakDetectionCheckImpl::RequestPayloadHelper::OnGotAccessToken(
    absl::optional<std::string> access_token) {
  access_token_ = std::move(access_token);
  steps_ |= kAccessToken;
  token_fetcher_.reset();

  CheckAllStepsDone();
}

void LeakDetectionCheckImpl::RequestPayloadHelper::OnGotPayload(
    LookupSingleLeakData data) {
  payload_ = std::move(data);
  steps_ |= kPayloadData;

  CheckAllStepsDone();
}

void LeakDetectionCheckImpl::RequestPayloadHelper::CheckAllStepsDone() {
  if (steps_ == kAll) {
    leak_check_->DoLeakRequest(std::move(payload_), std::move(access_token_),
                               std::move(api_key_),
                               std::move(url_loader_factory_));
  }
}

LeakDetectionCheckImpl::LeakDetectionCheckImpl(
    LeakDetectionDelegateInterface* delegate,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    absl::optional<std::string> api_key)
    : delegate_(delegate),
      payload_helper_(new RequestPayloadHelper(this,
                                               identity_manager,
                                               std::move(url_loader_factory),
                                               std::move(api_key))),
      network_request_factory_(
          std::make_unique<LeakDetectionRequestFactory>()) {
  DCHECK(delegate_);
}

LeakDetectionCheckImpl::~LeakDetectionCheckImpl() = default;

// static
bool LeakDetectionCheckImpl::HasAccountForRequest(
    const signin::IdentityManager* identity_manager) {
  // On desktop HasPrimaryAccount(signin::ConsentLevel::kSignin) will
  // always return something if the user is signed in.
  // On Android it will be empty if the user isn't syncing. Thus,
  // GetAccountsWithRefreshTokens() check is necessary.
  return identity_manager &&
         (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) ||
          !identity_manager->GetAccountsWithRefreshTokens().empty());
}

void LeakDetectionCheckImpl::Start(const GURL& url,
                                   std::u16string username,
                                   std::u16string password) {
  DCHECK(payload_helper_);
  DCHECK(!request_);

  url_ = url;
  username_ = std::move(username);
  password_ = std::move(password);
  if (HasAccountForRequest(payload_helper_->GetIdentityManager())) {
    payload_helper_->RequestAccessToken(TimeCallback(
        base::BindOnce(&LeakDetectionCheckImpl::OnAccessTokenRequestCompleted,
                       weak_ptr_factory_.GetWeakPtr()),
        "PasswordManager.LeakDetection.ObtainAccessTokenTime"));
  } else {
    payload_helper_->OnGotAccessToken(/*access_token=*/absl::nullopt);
  }
  payload_helper_->PreparePayload(
      base::UTF16ToUTF8(username_), base::UTF16ToUTF8(password_),
      base::BindOnce(&LeakDetectionCheckImpl::OnRequestDataReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LeakDetectionCheckImpl::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    // Network error codes are negative. See: src/net/base/net_error_list.h.
    DLOG(ERROR) << "Token request error: " << error.error_message();
    delegate_->OnError(LeakDetectionError::kTokenRequestFailure);
    return;
  }

  // The fetcher successfully obtained an access token.
  DVLOG(0) << "Token=" << access_token_info.token;
  payload_helper_->OnGotAccessToken(std::move(access_token_info.token));
}

void LeakDetectionCheckImpl::OnRequestDataReady(LookupSingleLeakData data) {
  if (data.encryption_key.empty()) {
    DLOG(ERROR) << "Preparing the payload for leak  detection failed";
    delegate_->OnError(LeakDetectionError::kHashingFailure);
    return;
  }
  payload_helper_->OnGotPayload(std::move(data));
}

void LeakDetectionCheckImpl::DoLeakRequest(
    LookupSingleLeakData data,
    absl::optional<std::string> access_token,
    absl::optional<std::string> api_key,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  payload_helper_.reset();
  encryption_key_ = std::move(data.encryption_key);
  request_ = network_request_factory_->CreateNetworkRequest();
  request_->LookupSingleLeak(
      url_loader_factory.get(), access_token, api_key, std::move(data.payload),
      TimeCallback(
          base::BindOnce(&LeakDetectionCheckImpl::OnLookupSingleLeakResponse,
                         weak_ptr_factory_.GetWeakPtr()),
          "PasswordManager.LeakDetection.ReceiveSingleLeakResponseTime"));
}

void LeakDetectionCheckImpl::OnLookupSingleLeakResponse(
    std::unique_ptr<SingleLookupResponse> response,
    absl::optional<LeakDetectionError> error) {
  request_.reset();
  if (!response) {
    delegate_->OnError(*error);
    return;
  }

  DVLOG(0) << "Leak check: number of matching encrypted prefixes="
           << response->encrypted_leak_match_prefixes.size();

  AnalyzeResponse(
      std::move(response), encryption_key_,
      base::BindOnce(&LeakDetectionCheckImpl::OnAnalyzeSingleLeakResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LeakDetectionCheckImpl::OnAnalyzeSingleLeakResponse(
    AnalyzeResponseResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.LeakDetection.AnalyzeSingleLeakResponseResult", result);
  const bool is_leaked = result == AnalyzeResponseResult::kLeaked;
  DVLOG(0) << "Leak check result=" << is_leaked;
  delegate_->OnLeakDetectionDone(is_leaked, std::move(url_),
                                 std::move(username_), std::move(password_));
}

}  // namespace password_manager
