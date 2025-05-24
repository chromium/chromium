// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_H_

#include <optional>

#include "base/debug/stack_trace.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/country_codes/country_codes.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"

namespace policy {
class PolicyService;
}
namespace variations {
class VariationsService;
}
namespace regional_capabilities {
class RegionalCapabilitiesService;
}
namespace TemplateURLPrepopulateData {
class Resolver;
}

class PrefRegistrySimple;
class PrefService;
class TemplateURLService;

namespace search_engines {

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
      TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver);
  ~SearchEngineChoiceService() override;

  // Returns the choice screen eligibility condition most relevant for the
  // profile associated with `profile_prefs` and `template_url_service`. Only
  // checks dynamic conditions, that can change from one call to the other
  // during a profile's lifetime. Should be checked right before showing a
  // choice screen.
  SearchEngineChoiceScreenConditions GetDynamicChoiceScreenConditions(
      const TemplateURLService& template_url_service);

  // Returns the choice screen eligibility condition most relevant for the
  // profile described by `profile_properties`. Only checks static conditions,
  // such that if a non-eligible condition is returned, it would take at least a
  // restart for the state to change. So this state can be checked and cached
  // ahead of showing a choice screen.
  // TODO(b/318801987): Remove `is_regular_profile` after fixing tests.
  SearchEngineChoiceScreenConditions GetStaticChoiceScreenConditions(
      const policy::PolicyService& policy_service,
      bool is_regular_profile,
      const TemplateURLService& template_url_service);

  // Returns key information needed to show a search engine choice screen, like
  // the template URLs for the engines to show. See
  // `search_engines::ChoiceScreenData` for more details.
  std::unique_ptr<search_engines::ChoiceScreenData> GetChoiceScreenData(
      const SearchTermsData& search_terms_data);

  // Records that the choice was made by settings the timestamp if applicable.
  // Records the location from which the choice was made and the search engine
  // that was chosen.
  // The function should be called after the default search engine has been set.
  void RecordChoiceMade(ChoiceMadeLocation choice_location,
                        TemplateURLService* template_url_service);

  // Records metrics about what was displayed on the choice screen for this
  // profile, as captured by `display_state`.
  // `is_from_cached_state` being `true` indicates that this is not the first
  // time the method has been called for this profile, and that we are now
  // calling it with some `display_state` that was cached from a previous
  // attempt due to a mismatch between the Variations country and the one
  // associated with the profile. Some metrics can be logged right away, while
  // some others are logged only when the countries match.
  // Note that due to various constraints, this might end up being a no-op and
  // not record anything.
  void MaybeRecordChoiceScreenDisplayState(
      const ChoiceScreenDisplayState& display_state,
      bool is_from_cached_state = false);

  // Clear state e.g. when a guest session is closed.
  void ResetState();

  // Clears the country id cache to be able to change countries multiple times
  // in tests.
  // TODO(crbug.com/328040066): Move to `//components/regional_capabilities`.
  void ClearCountryIdCacheForTesting();

  // Returns a reference to the `SearchEngineChoiceService::Client` owned and
  // used by this service.
  Client& GetClientForTesting();

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

 private:
  // Checks if the search engine choice should be prompted again, based on
  // experiment parameters. If a reprompt is needed, some preferences related to
  // the choice are cleared, which triggers a reprompt on the next page load.
  void PreprocessPrefsForReprompt();

  void ProcessPendingChoiceScreenDisplayState();

  std::unique_ptr<Client> client_;
  const raw_ref<PrefService> profile_prefs_;
  const raw_ptr<PrefService> local_state_;
  const raw_ref<regional_capabilities::RegionalCapabilitiesService>
      regional_capabilities_service_;
  const raw_ref<TemplateURLPrepopulateData::Resolver>
      prepopulate_data_resolver_;
  base::ObserverList<Observer> observers_;

  // Used to ensure that the value returned from `GetCountryId` never changes
  // in runtime (different runs can still return different values, though).
  std::optional<int> country_id_cache_;

  // Used to track caller of `MaybeRecordChoiceScreenDisplayState()` to debug
  // some unmet expectations, see b/344899110.
  std::unique_ptr<base::debug::StackTrace> display_state_record_caller_;

  base::WeakPtrFactory<SearchEngineChoiceService> weak_ptr_factory_{this};
};

void MarkSearchEngineChoiceCompletedForTesting(PrefService& prefs);

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_SERVICE_H_
