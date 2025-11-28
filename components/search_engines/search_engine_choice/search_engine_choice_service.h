// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/country_codes/country_codes.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/regional_capabilities/program_settings.h"

namespace policy {
class ManagementService;
class PolicyService;
}
namespace signin {
class IdentityManager;
}
namespace variations {
class VariationsService;
}
namespace regional_capabilities {
class RegionalCapabilitiesService;
struct ChoiceScreenEligibilityConfig;
}  // namespace regional_capabilities
namespace TemplateURLPrepopulateData {
class Resolver;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefRegistrySimple;
class PrefService;
class SearchTermsData;
class TemplateURL;
class TemplateURLService;

namespace search_engines {

class ChoiceScreenData;
class SearchEngineChoiceService;
enum class ChoiceMadeLocation;
enum class SearchEngineChoiceScreenConditions;
enum class SearchEngineChoiceScreenEvents;
enum class SearchEngineChoiceWipeReason;
struct ChoiceCompletionMetadata;
struct ChoiceScreenDisplayState;

// `KeyedService` for managing the state related to Search Engine Choice (mostly
// for the country information).
class SearchEngineChoiceService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSavedGuestSearchChanged() = 0;
  };

  // Interface allowing SearchEngineChoiceService to have access to
  // dependencies from higher level layers or that's can't be passed in
  // at construction time, for example due to incompatible lifecycles.
  class Client {
   public:
    virtual ~Client();

    // Returns the Variations (Finch) country ID for this current run, or an
    // invalid country ID if it's not available.
    virtual country_codes::CountryId GetVariationsCountry() = 0;

    // Returns whether this profile type is compatible with the
    // Guest-specific default search engine propagation.
    virtual bool IsProfileEligibleForDseGuestPropagation() = 0;

    // Returns whether Chrome detected in this current run that its data has
    // been transferred / restored to a new device.
    //
    // In practice, this function is not reliable on desktop. That's because
    // "detected in current session" happens asynchronously, so it's possible
    // to call this function and get a "false" value in a session where it will
    // end up returning true at some point. And in the next session, "detected
    // in current session" would be false too. It's possible to miss an actual
    // true value due to timing of calls to this function.
    virtual bool IsDeviceRestoreDetectedInCurrentSession() = 0;

    // Returns whether the search engine choice described in `choice_metadata`
    // predates the Chrome data having been transferred or restored to this
    // device.
    virtual bool DoesChoicePredateDeviceRestore(
        const ChoiceCompletionMetadata& choice_metadata) = 0;

   protected:
    // Helper for subclass to have the possibility to share some of the
    // implementation of `GetVariationsCountry()`.
    static country_codes::CountryId GetVariationsLatestCountry(
        variations::VariationsService* variations_service);
  };

  SearchEngineChoiceService(
      std::unique_ptr<Client> client,
      PrefService& profile_prefs,
      PrefService* local_state,
      regional_capabilities::RegionalCapabilitiesService& regional_capabilities,
      TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver,
      signin::IdentityManager& identity_manager,
      policy::ManagementService& platform_management_service);
  ~SearchEngineChoiceService() override;

  // Runs the initialisation step for this service, checking consistency in the
  // prefs and performing some tasks that might be needed following device state
  // changes.
  void Init();

  // Returns the choice screen eligibility condition most relevant for the
  // profile associated with `profile_prefs` and `template_url_service`. Only
  // checks dynamic conditions, that can change from one call to the other
  // during a profile's lifetime. Should be checked right before showing a
  // choice screen.
  SearchEngineChoiceScreenConditions GetDynamicChoiceScreenConditions(
      const TemplateURLService& template_url_service) const;

  // Returns the choice screen eligibility condition most relevant for the
  // profile described by `profile_properties`. Only checks static conditions,
  // such that if a non-eligible condition is returned, it would take at least a
  // restart for the state to change. So this state can be checked and cached
  // ahead of showing a choice screen.
  SearchEngineChoiceScreenConditions GetStaticChoiceScreenConditions(
      const policy::PolicyService& policy_service,
      const TemplateURLService& template_url_service) const;

  // Records the specified choice screen condition at profile initialization.
  void RecordProfileLoadEligibility(
      SearchEngineChoiceScreenConditions condition);

#if BUILDFLAG(IS_IOS)
  // Records only the legacy static eligibility histograms. Note that on iOS,
  // the legacy histograms are not recorded by `RecordProfileLoadEligibility()`
  void RecordLegacyStaticEligibility(
      SearchEngineChoiceScreenConditions condition);

  // Indicates whether the choice screen can be shown on a surface with a
  // particular "first run experience" status.
  bool IsSurfaceEligible(bool is_first_run_experience_surface) const;
#endif  // BUILDFLAG(IS_IOS)

  // Records the specified choice screen condition for relevant navigations.
  void RecordTriggeringEligibility(
      SearchEngineChoiceScreenConditions condition);

  // Records the specified choice screen event.
  void RecordChoiceScreenEvent(SearchEngineChoiceScreenEvents event);

  // Returns key information needed to show a search engine choice screen, like
  // the template URLs for the engines to show. See
  // `search_engines::ChoiceScreenData` for more details. `nullptr` values for
  // `default_search_provider` are accepted as default search might be disabled.
  std::unique_ptr<search_engines::ChoiceScreenData> GetChoiceScreenData(
      const SearchTermsData& search_terms_data,
      const TemplateURL* default_search_provider);

  // Records that the choice was made by settings the timestamp if applicable.
  // Records the location from which the choice was made and the search engine
  // that was chosen.
  // The function should be called after the default search engine has been set.
  void RecordChoiceMade(ChoiceMadeLocation choice_location,
                        TemplateURLService* template_url_service);

  // Records metrics about what was displayed on the choice screen for this
  // profile, as captured by `display_state`.
  // If due to various constraints, the metrics can't be fully recorded, the
  // state is cached and the service will attempt it the next time it is
  // initialized.
  void MaybeRecordChoiceScreenDisplayState(
      const ChoiceScreenDisplayState& display_state);

  // Clear state e.g. when a guest session is closed.
  void ResetState();

  // Returns a reference to the `SearchEngineChoiceService::Client` owned and
  // used by this service.
  Client& GetClientForTesting();

  enum class ChoiceStatus {
    // Metadata indicates that a search engine choice has been made and is
    // considered valid.
    kValid,
    // No search engine choice has been made yet.
    kNotMade,
    // The current search engine choice has been made on a different device.
    kFromRestoredDevice,
    // There is no default search provider available, likely disabled by
    // enterprise policies.
    kDefaultSearchDisabled,
    // The current default search provider is set by enterprise policies.
    kCurrentIsSetByPolicy,
    // The current default search provider is set by an extension.
    kCurrentIsSetByExtension,
    // The current default search provider is non-Google prepopulated one.
    kCurrentIsNonGooglePrepopulated,
    // The current default search provider is a custom, client-specified URL.
    // For example, it could be entered manually by the user or picked up as
    // site search.
    kCurrentIsNotPrepopulated,
    // The current default search provider is coming from search provider
    // overrides set by the admin or non-standard distribution channel.
    kCurrentIsDistributionCustom,
    // The current default search provider has a prepopulated ID that doesn't
    // match any of the preopulated engines currently available.
    kCurrentIsUnknownPrepopulated,
    // The user is not eligible for the choice screen based on their account
    // capabilities.
    kAccountNotEligible,
    // The device is not eligible for the choice screen based on its management
    // status.
    kManaged,
  };
  ChoiceStatus EvaluateSearchProviderChoiceForTesting(
      const TemplateURLService& template_url_service);

  // Returns whether the profile is eligible for the default search engine to be
  // used across all guest sessions.
  bool IsDsePropagationAllowedForGuest() const;

  // Returns the previously chosen default search engine configured to be
  // propagated to new guest sessions. Returns nullopt if the profile is
  // not eligible for DSE propagation or no DSE choice was previously stored.
  std::optional<int> GetSavedSearchEngineBetweenGuestSessions() const;

  // Save the `prepopulated_id` of the chosen search engine to be used for all
  // guest sessions. Pass nullopt to reset the search engine choice.
  void SetSavedSearchEngineBetweenGuestSessions(
      std::optional<int> prepopulated_id);

  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }

  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

  // Register Local state preferences in `registry`.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Register profile preferences in `registry`.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // Checks if the search engine choice should be invalidated, based on pref
  // inconsistencies, command line args, or experiment parameters. Returns a
  // wipe reason if the choice should be cleared, or nullopt otherwise.
  std::optional<SearchEngineChoiceWipeReason> CheckPrefsForWipeReason();

  void ProcessPendingChoiceScreenDisplayState();

  enum class ChoiceRenewalReason {
    kOutdated,
    kIncompatibleProgram,

    kMin = kOutdated,
    kMax = kIncompatibleProgram,
  };

  using ChoiceRenewalReasons = base::EnumSet<ChoiceRenewalReason,
                                             ChoiceRenewalReason::kMin,
                                             ChoiceRenewalReason::kMax>;

  // Returns the reasons why the current choice should be renewed.
  ChoiceRenewalReasons GetChoiceRenewalReasons(
      const regional_capabilities::ChoiceScreenEligibilityConfig&
          eligibility_config,
      const ChoiceCompletionMetadata& completion_metadata) const;

  ChoiceStatus EvaluateSearchProviderChoice(
      const TemplateURLService& template_url_service) const;

  const std::unique_ptr<Client> client_;
  const raw_ref<PrefService> profile_prefs_;
  const raw_ptr<PrefService> local_state_;
  const raw_ref<regional_capabilities::RegionalCapabilitiesService>
      regional_capabilities_service_;
  const raw_ref<TemplateURLPrepopulateData::Resolver>
      prepopulate_data_resolver_;
  const raw_ref<signin::IdentityManager> identity_manager_;
  const raw_ref<policy::ManagementService> platform_management_service_;
  base::ObserverList<Observer> observers_;

  // Used to track whether `MaybeRecordChoiceScreenDisplayState()` has already
  // been called for this profile, to monitor the prevalence of some unexpected
  // behaviour, see crbug.com/390272573.
  bool has_recorded_display_state_ = false;

  base::WeakPtrFactory<SearchEngineChoiceService> weak_ptr_factory_{this};
};

void MarkSearchEngineChoiceCompletedForTesting(
    PrefService& prefs,
    ChoiceCompletionMetadata metadata);

void MarkSearchEngineChoiceCompletedForTesting(
    PrefService& prefs,
    regional_capabilities::Program program =
        regional_capabilities::Program::kWaffle);

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_H_
