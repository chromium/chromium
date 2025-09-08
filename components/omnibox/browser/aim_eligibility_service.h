// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"

class AimEligibilityServiceObserver;
class PrefRegistrySimple;
class PrefService;
class TemplateURLService;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// If enabled, uses the server response for AIM eligibility for all locales.
BASE_DECLARE_FEATURE(kAimServerEligibilityEnabled);

// Utility service to check if the profile is eligible for AI mode features.
class AimEligibilityService : public KeyedService {
 public:
  // See comment for `WriteToPref()`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  // Returns true if the AIM is allowed per the policy.
  static bool IsAimAllowedByPolicy(const PrefService* prefs);

  AimEligibilityService(
      PrefService& pref_service,
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AimEligibilityService() override;

  // Checks if the application country matches the given country.
  bool IsCountry(const std::string& country) const;
  // Checks if the application language matches the given language.
  bool IsLanguage(const std::string& language) const;

  // Register observers to notify when eligibility may have changed. Eligibility
  // is re-checked periodically.
  void AddObserver(AimEligibilityServiceObserver* observer);
  void RemoveObserver(AimEligibilityServiceObserver* observer);

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

 protected:
  // Virtual methods for platform-specific country and locale access.
  virtual std::string GetCountryCode() const = 0;
  virtual std::string GetLocale() const = 0;

 private:
  friend class AimEligibilityServiceFriend;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(ServerAimEligibilityRequestStatus)
  enum class ServerRequestStatus {
    kSent = 0,
    kErrorResponse = 1,
    kFailedToParse = 2,
    kSuccess = 3,
    kMaxValue = kSuccess,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:ServerAimEligibilityRequestStatus)

  // Initializes the service.
  void Initialize();

  // Notify `observers_` ineligibles may have changed.
  void NotifyObservers() const;

  // Updates `most_recent_response_` and the prefs with `response_proto`.
  void UpdateMostRecentResponse(
      const omnibox::AimEligibilityResponse& response_proto);
  // Loads `most_recent_response_` from the prefs, if valid.
  void LoadMostRecentResponse();

  // Fetch eligibility from the server.
  void StartServerEligibilityRequest();
  void OnServerEligibilityResponse(
      std::unique_ptr<network::SimpleURLLoader> loader,
      std::unique_ptr<std::string> response_string);

  static constexpr char kResponsePrefName[] =
      "aim_eligibility_service.aim_eligibility_response";

  base::ObserverList<AimEligibilityServiceObserver> observers_;
  const raw_ref<PrefService> pref_service_;
  // Outlives `this` due to BCKSF dependency. Can be nullptr in tests.
  const raw_ptr<TemplateURLService> template_url_service_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::CallbackListSubscription template_url_service_subscription_;

  // Update on each successfully server response.
  omnibox::AimEligibilityResponse most_recent_response_;
  // Tracks whether the service has been initialized.
  bool initialized_ = false;

  // For binding the `OnServerEligibilityResponse()` callback.
  base::WeakPtrFactory<AimEligibilityService> weak_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_H_
