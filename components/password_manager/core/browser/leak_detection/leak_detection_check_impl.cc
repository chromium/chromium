// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_check_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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
      std::optional<std::string> api_key);
  ~RequestPayloadHelper() = default;

  // Neither copyable nor movable.
  RequestPayloadHelper(const RequestPayloadHelper&) = delete;
  RequestPayloadHelper& operator=(const RequestPayloadHelper&) = delete;
  RequestPayloadHelper(RequestPayloadHelper&&) = delete;
  RequestPayloadHelper& operator=(RequestPayloadHelper&&) = delete;

  // Returns |identity_manager_| for the profile.
  signin::IdentityManager* GetIdentityManager();

  void RequestAccessToken(AccessTokenFetcher::TokenCallback callback);
  void PreparePayload(LeakDetectionInitiator initiator,
                      const std::string& username,
                      const std::string& password,
                      SingleLeakRequestDataCallback callback);

  // Notifies that the access token was obtained.
  void OnGotAccessToken(std::optional<std::string> access_token);

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
  // |std::nullopt| for signed-out users.
  std::optional<std::string> access_token_;
  // Api key required to authenticate signed-out user. It should be
  // |std::nullopt| for signed-in users.
  const std::optional<std::string> api_key_;
  // Payload for the actual request.
  LookupSingleLeakData payload_;
};

LeakDetectionCheckImpl::RequestPayloadHelper::RequestPayloadHelper(
    LeakDetectionCheckImpl* leak_check,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::optional<std::string> api_key)
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
    LeakDetectionInitiator initiator,
    const std::string& username,
    const std::string& password,
    SingleLeakRequestDataCallback callback) {
  PrepareSingleLeakRequestData(initiator, username, password,
                               std::move(callback));
}

void LeakDetectionCheckImpl::RequestPayloadHelper::OnGotAccessToken(
    std::optional<std::string> access_token) {
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
    std::optional<std::string> api_key)
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
  return identity_manager &&
         identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

void LeakDetectionCheckImpl::Start(LeakDetectionInitiator initiator,
                                   const GURL& url,
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
    payload_helper_->OnGotAccessToken(/*access_token=*/std::nullopt);
  }
  payload_helper_->PreparePayload(
      initiator, base::UTF16ToUTF8(username_), base::UTF16ToUTF8(password_),
      base::BindOnce(&LeakDetectionCheckImpl::OnRequestDataReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

// static
bool LeakDetectionCheck::CanStartLeakCheck(
    const PrefService& prefs,
    const GURL& form_url,
    std::unique_ptr<autofill::SavePasswordProgressLogger> logger) {
  const bool is_leak_protection_on =
      prefs.GetBoolean(prefs::kPasswordLeakDetectionEnabled);
  // Leak detection can only start if:
  // 1. The user has not opted out and Safe Browsing is turned on, or
  // 2. The user is an enhanced protection user
  safe_browsing::SafeBrowsingState sb_state =
      safe_browsing::GetSafeBrowsingState(prefs);
  switch (sb_state) {
    case safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING:
      if (logger) {
        logger->LogMessage(autofill::SavePasswordProgressLogger::
                               STRING_LEAK_DETECTION_DISABLED_SAFE_BROWSING);
      }
      return false;
    case safe_browsing::SafeBrowsingState::STANDARD_PROTECTION:
      if (!is_leak_protection_on && logger) {
        logger->LogMessage(autofill::SavePasswordProgressLogger::
                               STRING_LEAK_DETECTION_DISABLED_FEATURE);
      }
      return is_leak_protection_on && !LeakDetectionCheck::IsURLBlockedByPolicy(
                                          prefs, form_url, logger.get());
    case safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION:
      return !LeakDetectionCheck::IsURLBlockedByPolicy(prefs, form_url,
                                                       logger.get());
  }
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
    std::optional<std::string> access_token,
    std::optional<std::string> api_key,
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
    std::optional<LeakDetectionError> error) {
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

bool LeakDetectionCheck::IsURLBlockedByPolicy(
    const PrefService& prefs,
    const GURL& form_url,
    autofill::SavePasswordProgressLogger* logger) {
  bool is_blocked = safe_browsing::IsURLAllowlistedByPolicy(form_url, prefs);
  if (is_blocked && logger) {
    logger->LogMessage(autofill::SavePasswordProgressLogger::
                           STRING_LEAK_DETECTION_URL_BLOCKED);
  }
  return is_blocked;
}

}  // namespace password_manager
