// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/network_change_notifier.h"
#include "third_party/omnibox_proto/aim_eligibility_client_request.pb.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"

class PrefRegistrySimple;
class PrefService;
class TemplateURLService;

namespace base {
struct Feature;
}

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
struct ResourceRequest;
}  // namespace network

// Utility service to check if the profile is eligible for AI mode features.
class AimEligibilityService
    : public KeyedService,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public signin::IdentityManager::Observer,
      public TemplateURLServiceObserver {
 public:
  // Helper that individual AIM features can use to check if they should be
  // enabled. Unlike most chrome features, which simply check if the
  // `base::Feature` is enabled, AIM features should use this so that they
  // auto-launch them when the eligibility service launches.
  // This should not be used for new AIM features so that they don't affect
  // ineligible users (`GenericKillSwitchFeatureCheck()` was intended for
  // pre-eligibility-service features that wanted to conditionally ignore
  // eligibility).
  static bool GenericKillSwitchFeatureCheck(
      const AimEligibilityService* aim_eligibility_service,
      const base::Feature& feature,
      const std::optional<std::reference_wrapper<const base::Feature>>
          feature_en_us = std::nullopt);
  // See comment for `WriteToPref()`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  // Returns true if the AIM is allowed per the policy.
  static bool IsAimAllowedByPolicy(const PrefService* prefs);

  // Tracks the source of `most_recent_response_`.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(EligibilityResponseSource)
  enum class EligibilityResponseSource {
    kDefault = 0,
    kPrefs = 1,
    kServer = 2,
    kBrowserCache = 3,
    kUser = 4,
    kMaxValue = kUser,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:AimEligibilityResponseSource)

  // Converts EligibilityResponseSource enum to string.
  static std::string EligibilityResponseSourceToString(
      EligibilityResponseSource source);

  // Enum describing the eligibility request mode.
  enum class ServerEligibilityRequestMode {
    kLegacyGet = 0,
    kGetWithLocale = 1,
    kPostWithProto = 2,
  };

  // Configuration object for the service.
  struct Configuration {
    // Whether the profile is off-the-record.
    bool is_off_the_record = false;

    // The value for the `User-Agent` header when Co-Browse is enabled. The
    // enabled / disabled state refers to the feature flag for Co-Browse, not
    // whether this client is Co-Browse eligible.
    std::string user_agent_with_cobrowse_suffix;

    // The value for the `Sec-CH-UA-Full-Version-List` HTTP Header. The header
    // is skipped if it is empty.
    std::string full_version_list;
  };

  // Returns the current server eligibility request mode based on the feature
  // flag configuration.
  static ServerEligibilityRequestMode GetServerEligibilityRequestMode();

  AimEligibilityService(
      PrefService& pref_service,
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const std::string& locale,
      Configuration configuration);
  ~AimEligibilityService() override;

  // Checks if the application country matches the given country.
  bool IsCountry(const std::string& country) const;
  // Checks if the application language matches the given language.
  bool IsLanguage(const std::string& language) const;

  // Registers a callback to be called when eligibility has changed.
  // Virtual for testing purposes.
  [[nodiscard]] virtual base::CallbackListSubscription
  RegisterEligibilityChangedCallback(base::RepeatingClosure callback);

  // Checks if server eligibility checking is enabled.
  // Virtual for testing purposes.
  virtual bool IsServerEligibilityEnabled() const;

  // Checks if AIM is allowed by default search engine (Google DSE).
  // Virtual for testing purposes.
  virtual bool IsAimAllowedByDse() const;

  // Checks if user is locally eligible for AI mode (excludes server checks).
  // Virtual for testing purposes.
  virtual bool IsAimLocallyEligible() const;

  // Checks if user is eligible for AI mode (includes server checks).
  // Virtual for testing purposes.
  virtual bool IsAimEligible() const;

  // Checks if user is eligible for Pdf Upload in AIM features.
  // Virtual for testing purposes.
  virtual bool IsPdfUploadEligible() const;

  // Checks if user is eligible for Deep Search in AIM features.
  virtual bool IsDeepSearchEligible() const;

  // Checks if user is eligible for Create Images in AIM features. Always
  // returns false for off-the-record profiles.
  virtual bool IsCreateImagesEligible() const;

  // Checks if user is eligible for Canvas in AIM features.
  virtual bool IsCanvasEligible() const;

  // Checks if the user is eligible for Co-Browse in AIM features.
  virtual bool IsCobrowseEligible() const;

  // Determining whether the provided URL is an AI page based on server-provided
  // params.
  virtual bool HasAimUrlParams(const GURL& url) const;

  // Returns the most recent eligibility response proto.
  virtual const omnibox::AimEligibilityResponse& GetMostRecentResponse() const;

  // Returns the source of the most recent eligibility response.
  EligibilityResponseSource GetMostRecentResponseSource() const;

  // Returns the `SearchboxConfig` from the AIMEligibilityResponse.
  virtual const omnibox::SearchboxConfig* GetSearchboxConfig() const;

  // NOTE: Following methods are intended for chrome://aim-eligibility-internals
  // for debugging purposes only:
  // Triggers a server request to fetch eligibility from the server.
  void StartServerEligibilityRequestForDebugging();
  // Sets the eligibility response directly from a base64-encoded string.
  // Returns true if the response was successfully decoded and saved.
  bool SetEligibilityResponseForDebugging(
      const std::string& base64_encoded_response);

  // Tracks the source of the eligibility request.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(RequestSource)
  enum class RequestSource {
    kStartup = 0,
    kCookieChange = 1,
    kPrimaryAccountChange = 2,
    kNetworkChange = 3,
    kUser = 4,
    kAimUrlNavigation = 5,
    kRefreshTokenUpdated = 6,
    kRefreshTokenRemoved = 7,
    kRefreshTokenError = 8,
    kOAuthFallbackCookieChange = 9,
    kMaxValue = kOAuthFallbackCookieChange,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/histograms.xml:AimEligibilityRequestSource)

  // Triggers a server request to fetch eligibility from the server.
  virtual void FetchEligibility(RequestSource source);

 protected:
  // Virtual methods for platform-specific country and locale access.
  virtual std::string GetCountryCode() const = 0;
  virtual std::string GetLocale() const = 0;

 private:
  friend class AimEligibilityServiceFriend;

  // Converts RequestSource enum to histogram suffix string.
  static std::string RequestSourceToString(RequestSource source);

  // Tracks the status of the eligibility request.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(EligibilityRequestStatus)
  enum class EligibilityRequestStatus {
    kSent = 0,
    kErrorResponse = 1,
    kFailedToParse = 2,
    kSuccess = 3,
    kSuccessBrowserCache = 4,
    kMaxValue = kSuccessBrowserCache,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:AimEligibilityRequestStatus)

  // Returns server eligibility if the feature is AIM eligible.
  bool IsEligibleByServer(bool server_eligibility) const;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // net::NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // Callback for when the DSE changes.
  void OnDseChanged();

  // Callback for when the AIM policy changes.
  void OnPolicyChanged();

  // Callback for when the eligibility response changes. Notifies observers.
  void OnEligibilityResponseChanged();

  // Updates `most_recent_response_` and the prefs with `response_proto`.
  void UpdateMostRecentResponse(
      const omnibox::AimEligibilityResponse& response_proto,
      EligibilityResponseSource response_source);

  // Loads `most_recent_response_` from the prefs, if valid.
  void LoadMostRecentResponse();

  // Returns whether the primary account is valid and can be used for OAuth.
  bool HasValidPrimaryAccount() const;

  // Configures the `request` credentials and cookies based on `use_oauth` and
  // the `kAimEligibilityServiceIdentityImprovements` feature.
  void ConfigureRequestCookiesAndCredentials(network::ResourceRequest* request,
                                             bool use_oauth) const;

  // Returns the active account (Primary or Cookie fallback) for eligibility
  // checks.
  GaiaId GetActiveAccount() const;

  // Returns the first account in the cookie jar if valid and signed in.
  GaiaId GetFirstAccountInCookieJarIfValid() const;

  // Returns true if the request should be dropped.
  bool ShouldDropRequest() const;

  // Queues a request if the last active account changed.
  void ScheduleServerEligibilityRequestIfNeeded(RequestSource source);

  // Returns the request URL or an empty GURL if a valid URL cannot be created;
  // e.g., Google is not the default search provider.
  GURL GetRequestUrl(RequestSource request_source,
                     const TemplateURLService* template_url_service,
                     signin::IdentityManager* identity_manager,
                     const std::string& locale);

  // Schedules the server request to execute after a fixed period to ensure that
  // a burst of rapid calls results in only a single server request within the
  // debounce period.
  void ScheduleServerEligibilityRequest(RequestSource request_source,
                                        const std::string& locale);

  // Starts a server eligibility request, first fetching an access token if
  // OAuth is enabled and the user is logged in.
  void StartServerEligibilityRequest(RequestSource request_source,
                                     const std::string& locale);

  // Callback for when an access token is available.
  void OnAccessTokenAvailable(RequestSource request_source,
                              const std::string& locale,
                              GaiaId pending_request_account,
                              std::unique_ptr<network::ResourceRequest> request,
                              GoogleServiceAuthError error,
                              signin::AccessTokenInfo access_token_info);

  // Sends a server eligibility request, triggered after receiving an access
  // token or directly by `StartServerEligibilityRequest` if OAuth is disabled.
  void SendServerEligibilityRequest(
      RequestSource request_source,
      const std::string& locale,
      GaiaId pending_request_account,
      std::unique_ptr<network::ResourceRequest> request);
  void OnServerEligibilityResponse(RequestSource request_source,
                                   GaiaId pending_request_account,
                                   std::optional<std::string> response_string);
  void ProcessServerEligibilityResponse(
      RequestSource request_source,
      GaiaId pending_request_account,
      int response_code,
      EligibilityRequestStatus request_status,
      int num_retries,
      std::optional<std::string> response_string);

  // Returns the given histogram name sliced by the given request source.
  std::string GetHistogramNameSlicedByRequestSource(
      const std::string& name,
      RequestSource request_source) const;
  // Records total and sliced histograms for whether the primary account exists.
  void LogEligibilityRequestPrimaryAccountExists(
      bool exists,
      RequestSource request_source) const;
  // Records total and sliced histograms for whether the primary account was
  // found in the cookie jar.
  void LogEligibilityRequestPrimaryAccountInCookieJar(
      bool in_cookie_jar,
      RequestSource request_source) const;
  // Records total and sliced histograms for the index of the primary account
  // in the cookie jar, if found.
  void LogEligibilityRequestPrimaryAccountIndex(
      size_t session_index,
      RequestSource request_source) const;
  // Records total and sliced histograms for OAuth token fetch status.
  void LogEligibilityRequestOAuthTokenFetchStatus(
      GoogleServiceAuthError::State state,
      RequestSource request_source) const;
  // Records total and sliced histograms for whether the OAuth token was
  // provided.
  void LogEligibilityRequestOAuthTokenProvided(
      bool has_token,
      RequestSource request_source) const;
  // Records total and sliced histograms for OAuth fallback.
  void LogEligibilityRequestOAuthFallback(bool fallback_happened,
                                          RequestSource request_source) const;
  // Records total and sliced histograms for eligibility request status.
  void LogEligibilityRequestStatus(EligibilityRequestStatus status,
                                   RequestSource request_source) const;
  // Record total and sliced histograms for eligibility request response code.
  void LogEligibilityRequestResponseCode(int response_code,
                                         RequestSource request_source) const;
  // Record total and sliced histograms for eligibility response.
  void LogEligibilityResponse(RequestSource request_source) const;
  // Record histograms for eligibility response changes.
  void LogEligibilityResponseChanges(
      const omnibox::AimEligibilityResponse& old_response,
      const omnibox::AimEligibilityResponse& new_response) const;

  // Records histogram for eligibility request debounced.
  void LogEligibilityRequestDebounced(bool is_debounced,
                                      RequestSource request_source) const;

  // Records histogram for whether the eligibility response account mismatches
  // the current active account.
  void LogEligibilityResponseAccountMismatch(
      bool response_account_mismatch,
      RequestSource request_source) const;

  const raw_ref<PrefService, DanglingUntriaged> pref_service_;
  // Outlives `this` due to BCKSF dependency. Can be nullptr in tests.
  raw_ptr<TemplateURLService> template_url_service_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Outlives `this` due to BCKSF dependency. Can be nullptr in tests.
  const raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_;
  bool is_dse_google_ = false;

  PrefChangeRegistrar pref_change_registrar_;
  base::CallbackListSubscription template_url_service_subscription_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::RepeatingClosureList eligibility_changed_callbacks_;

  // Updated on service initialization and on successful server response.
  omnibox::AimEligibilityResponse most_recent_response_;
  EligibilityResponseSource most_recent_response_source_ =
      EligibilityResponseSource::kDefault;

  // The account associated with the most recent response.
  GaiaId most_recent_response_account_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // The active URL loader for the eligibility request.
  std::unique_ptr<network::SimpleURLLoader> active_loader_;

  // Tracks whether the startup request has been sent.
  bool startup_request_sent_ = false;

  // Used to debounce server eligibility requests to prevent multiple requests.
  base::OneShotTimer request_debounce_timer_;

  // Used to store the default config when the response doesn't have one.
  mutable omnibox::SearchboxConfig fallback_config_;

  // A configuration for the service.
  const Configuration configuration_;

  // For binding the `OnServerEligibilityResponse()` callback.
  base::WeakPtrFactory<AimEligibilityService> weak_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_H_
