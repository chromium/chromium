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
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/network_change_notifier.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"

class PrefRegistrySimple;
class PrefService;
class TemplateURLService;

namespace base {
struct Feature;
}

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// Utility service to check if the profile is eligible for AI mode features.
class AimEligibilityService
    : public KeyedService,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public signin::IdentityManager::Observer {
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

  AimEligibilityService(
      PrefService& pref_service,
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      bool is_off_the_record);
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

 protected:
  // Virtual methods for platform-specific country and locale access.
  virtual std::string GetCountryCode() const = 0;
  virtual std::string GetLocale() const = 0;

 private:
  friend class AimEligibilityServiceFriend;

  // Tracks the source of the eligibility request.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(RequestSource)
  enum class RequestSource {
    kStartup = 0,
    kCookieChange = 1,
    kPrimaryAccountChange = 2,
    kNetworkChange = 3,
    kMaxValue = kNetworkChange,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/histograms.xml:AimEligibilityRequestSource)

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

  // Tracks the source of `most_recent_response_`.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(EligibilityResponseSource)
  enum class EligibilityResponseSource {
    kDefault = 0,
    kPrefs = 1,
    kServer = 2,
    kBrowserCache = 3,
    kMaxValue = kBrowserCache,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:AimEligibilityResponseSource)

  // Initializes the service. This isn't inlined in the constructor because
  // initialization may have to be delayed until after `template_url_service_`
  // has loaded.
  void Initialize();

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // net::NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Callback for when the eligibility response changes. Notifies observers.
  void OnEligibilityResponseChanged();

  // Updates `most_recent_response_` and the prefs with `response_proto`.
  void UpdateMostRecentResponse(
      const omnibox::AimEligibilityResponse& response_proto,
      bool was_fetched_via_cache = false);
  // Loads `most_recent_response_` from the prefs, if valid.
  void LoadMostRecentResponse();

  // Returns the request URL or an empty GURL if a valid URL cannot be created;
  // e.g., Google is not the default search provider.
  GURL GetRequestUrl(RequestSource request_source,
                     const TemplateURLService* template_url_service,
                     signin::IdentityManager* identity_manager);

  // Fetch eligibility from the server.
  void StartServerEligibilityRequest(RequestSource request_source);
  void OnServerEligibilityResponse(
      std::unique_ptr<network::SimpleURLLoader> loader,
      RequestSource request_source,
      std::optional<std::string> response_string);
  void ProcessServerEligibilityResponse(
      RequestSource request_source,
      int response_code,
      bool was_fetched_via_cache,
      int num_retries,
      std::optional<std::string> response_string);

  // Returns true if AIM is allowed by policy and Google is the DSE.
  bool IsAimAllowedByPolicyAndDse() const;

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
  // Records total and sliced histograms for eligibility request status.
  void LogEligibilityRequestStatus(EligibilityRequestStatus status,
                                   RequestSource request_source) const;
  // Record total and sliced histograms for eligibility request response code.
  void LogEligibilityRequestResponseCode(int response_code,
                                         RequestSource request_source) const;
  // Record total and sliced histograms for eligibility response.
  void LogEligibilityResponse(RequestSource request_source) const;
  // Record histograms for eligibility response change.
  void LogEligibilityResponseChange() const;

  const raw_ref<PrefService, DanglingUntriaged> pref_service_;
  // Outlives `this` due to BCKSF dependency. Can be nullptr in tests.
  const raw_ptr<TemplateURLService, DanglingUntriaged> template_url_service_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Outlives `this` due to BCKSF dependency. Can be nullptr in tests.
  const raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_;
  const bool is_off_the_record_;

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

  // Tracks whether the service has been initialized.
  bool initialized_ = false;

  // Tracks whether the startup request has been sent.
  bool startup_request_sent_ = false;

  // For binding the `OnServerEligibilityResponse()` callback.
  base::WeakPtrFactory<AimEligibilityService> weak_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_H_
