// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_IMPL_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_IMPL_H_

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/remote/cached_image_fetcher.h"
#include "components/ntp_snippets/remote/json_to_categories.h"
#include "components/ntp_snippets/remote/prefetched_pages_tracker.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_status_service.h"
#include "components/ntp_snippets/remote/request_params.h"
#include "components/ntp_snippets/remote/request_throttler.h"

class PrefRegistrySimple;
class PrefService;

namespace image_fetcher {
class ImageFetcher;
}  // namespace image_fetcher

namespace ntp_snippets {

class CategoryRanker;
class RemoteSuggestionsDatabase;
class RemoteSuggestionsScheduler;

// Retrieves fresh content data (articles) from the server, stores them and
// provides them as content suggestions.
// This class is final because it does things in its constructor which make it
// unsafe to derive from it.
// TODO(treib): Introduce two-phase initialization and make the class not final?
class RemoteSuggestionsProviderImpl final : public RemoteSuggestionsProvider {
 public:
  // |application_language_code| should be a ISO 639-1 compliant string, e.g.
  // 'en' or 'en-US'. Note that this code should only specify the language, not
  // the locale, so 'en_US' (English language with US locale) and 'en-GB_US'
  // (British English person in the US) are not language codes.
  RemoteSuggestionsProviderImpl(
      Observer* observer,
      PrefService* pref_service,
      const std::string& application_language_code,
      CategoryRanker* category_ranker,
      RemoteSuggestionsScheduler* scheduler,
      std::unique_ptr<RemoteSuggestionsFetcher> suggestions_fetcher,
      std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
      std::unique_ptr<RemoteSuggestionsDatabase> database,
      std::unique_ptr<RemoteSuggestionsStatusService> status_service,
      std::unique_ptr<PrefetchedPagesTracker> prefetched_pages_tracker,
      std::unique_ptr<base::OneShotTimer> fetch_timeout_timer);

  ~RemoteSuggestionsProviderImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns whether the service is successfully initialized. While this is
  // false, some calls may trigger DCHECKs.
  bool initialized() const { return ready() || state_ == State::DISABLED; }

  // RemoteSuggestionsProvider implementation.
  void RefetchInTheBackground(FetchStatusCallback callback) override;
  void RefetchWhileDisplaying(FetchStatusCallback callback) override;
  // TODO(fhorschig): Remove this getter when there is an interface for the
  // fetcher that allows better mocks.
  const RemoteSuggestionsFetcher* suggestions_fetcher_for_debugging()
      const override;
  GURL GetUrlWithFavicon(
      const ContentSuggestion::ID& suggestion_id) const override;
  bool IsDisabled() const override;
  bool ready() const override;

  // ContentSuggestionsProvider implementation.
  CategoryStatus GetCategoryStatus(Category category) override;
  CategoryInfo GetCategoryInfo(Category category) override;
  void DismissSuggestion(const ContentSuggestion::ID& suggestion_id) override;
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            ImageFetchedCallback callback) override;
  void FetchSuggestionImageData(const ContentSuggestion::ID& suggestion_id,
                                ImageDataFetchedCallback callback) override;
  void Fetch(const Category& category,
             const std::set<std::string>& known_suggestion_ids,
             FetchDoneCallback callback) override;
  void ReloadSuggestions() override;
  void ClearHistory(
      base::Time begin,
      base::Time end,
      const base::Callback<bool(const GURL& url)>& filter) override;
  void ClearCachedSuggestions() override;
  void OnSignInStateChanged(bool has_signed_in) override;
  void GetDismissedSuggestionsForDebugging(
      Category category,
      DismissedSuggestionsCallback callback) override;
  void ClearDismissedSuggestionsForDebugging(Category category) override;

  // Returns the maximum number of suggestions we expect to receive from the
  // server during a normal (not fetch-more) fetch..
  static int GetMaxNormalFetchSuggestionCountForTesting();

  // Available suggestions, only for unit tests.
  // TODO(treib): Get rid of this. Tests should use a fake observer instead.
  const RemoteSuggestion::PtrVector& GetSuggestionsForTesting(
      Category category) const {
    return category_contents_.find(category)->second.suggestions;
  }

  // Dismissed suggestions, only for unit tests.
  const RemoteSuggestion::PtrVector& GetDismissedSuggestionsForTesting(
      Category category) const {
    return category_contents_.find(category)->second.dismissed;
  }

  // Overrides internal clock for testing purposes.
  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }

  // TODO(tschumann): remove this method as soon as we inject the fetcher into
  // the constructor.
  CachedImageFetcher& GetImageFetcherForTesting() { return image_fetcher_; }

 private:
  friend class RemoteSuggestionsProviderImplTest;

  // TODO(jkrcal): Mock the database to trigger the error naturally (or remove
  // the error state and get rid of the test).
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           CallsSchedulerOnError);
  // TODO(jkrcal): Mock the status service and remove these friend declarations.
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           CallsSchedulerWhenDisabled);
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           DontNotifyIfNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           CallsSchedulerWhenSignedIn);
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           CallsSchedulerWhenSignedOut);
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           RestartsFetchWhenSignedInWhileFetching);
  FRIEND_TEST_ALL_PREFIXES(RemoteSuggestionsProviderImplTest,
                           ShouldHandleCategoryDisabledBeforeTimeout);
  FRIEND_TEST_ALL_PREFIXES(
      RemoteSuggestionsProviderImplTest,
      ShouldNotSetExclusiveCategoryWhenFetchingSuggestions);

  // Possible state transitions:
  //       NOT_INITED --------+
  //       /       \          |
  //      v         v         |
  //   READY <--> DISABLED    |
  //       \       /          |
  //        v     v           |
  //     ERROR_OCCURRED <-----+
  // TODO(jkrcal): Do we need to keep the distinction between states DISABLED
  // and ERROR_OCCURED?
  enum class State {
    // The service has just been created. Can change to states:
    // - DISABLED: After the database is done loading,
    //             GetStateForDependenciesStatus can identify the next state to
    //             be DISABLED.
    // - READY: if GetStateForDependenciesStatus returns it, after the database
    //          is done loading.
    // - ERROR_OCCURRED: when an unrecoverable error occurred.
    NOT_INITED,

    // The service registered observers, timers, etc. and is ready to answer to
    // queries, fetch suggestions... Can change to states:
    // - DISABLED: when the global Chrome state changes, for example after
    //             |OnStateChanged| is called and sync is disabled.
    // - ERROR_OCCURRED: when an unrecoverable error occurred.
    READY,

    // The service is disabled and unregistered the related resources.
    // Can change to states:
    // - READY: when the global Chrome state changes, for example after
    //          |OnStateChanged| is called and sync is enabled.
    // - ERROR_OCCURRED: when an unrecoverable error occurred.
    DISABLED,

    // The service or one of its dependencies encountered an unrecoverable error
    // and the service can't be used anymore.
    ERROR_OCCURRED,

    COUNT
  };

  // Documents the status of the ongoing request and what action should be taken
  // on completion.
  enum class FetchRequestStatus {
    // There is no request in progress for remote suggestions.
    NONE,

    // There is a valid request in progress that should be treated normally on
    // completion.
    IN_PROGRESS,

    // There is a canceled request in progress. The response should be ignored
    // when it arrives.
    IN_PROGRESS_CANCELED,

    // There is an invalidated request in progress. On completion, we should
    // ignore the response and initiate a new fetch (with updated parameters).
    IN_PROGRESS_NEEDS_REFETCH
  };

  struct CategoryContent {
    // The current status of the category.
    CategoryStatus status = CategoryStatus::INITIALIZING;

    // The additional information about a category.
    CategoryInfo info;

    // TODO(vitaliii): Remove this field. It is always true, because we now
    // remove categories not included in the last fetch.
    // True iff the server returned results in this category in the last fetch.
    // We never remove categories that the server still provides.
    bool included_in_last_server_response = true;

    // All currently active suggestions (excl. the dismissed ones).
    RemoteSuggestion::PtrVector suggestions;

    // All previous suggestions that we keep around in memory because they can
    // be on some open NTP. We do not persist this list so that on a new start
    // of Chrome, this is empty.
    // |archived| is a FIFO buffer with a maximum length.
    base::circular_deque<std::unique_ptr<RemoteSuggestion>> archived;

    // Suggestions that the user dismissed. We keep these around until they
    // expire so we won't re-add them to |suggestions| on the next fetch.
    RemoteSuggestion::PtrVector dismissed;

    // Returns a non-dismissed suggestion with the given |id_within_category|,
    // or null if none exist.
    const RemoteSuggestion* FindSuggestion(
        const std::string& id_within_category) const;

    explicit CategoryContent(const CategoryInfo& info);
    CategoryContent(CategoryContent&&);
    ~CategoryContent();
    CategoryContent& operator=(CategoryContent&&);
  };

  // Fetches suggestions from the server and replaces old suggestions by the new
  // ones. Requests can be marked more important by setting
  // |interactive_request| to true (such request might circumvent the daily
  // quota for requests, etc.), useful for requests triggered by the user. After
  // the fetch finished, the provided |callback| will be triggered with the
  // status of the fetch.
  void FetchSuggestions(bool interactive_request, FetchStatusCallback callback);

  // Similar To FetchSuggestions, only adds a loading indicator on top of that.
  // If |enable_loading_indication_timeout| is true, the indicator is hidden if
  // the fetch does not finish within a certain amount of time (the fetch itself
  // is not canceled, though).
  void FetchSuggestionsWithLoadingIndicator(
      bool interactive_request,
      FetchStatusCallback callback,
      bool enable_loading_indication_timeout);
  void OnFetchSuggestionsWithLoadingIndicatorFinished(
      FetchStatusCallback callback,
      Status status);

  // Returns the URL of the image of a suggestion if it is among the current or
  // among the archived suggestions in the matching category. Returns an empty
  // URL otherwise.
  GURL FindSuggestionImageUrl(const ContentSuggestion::ID& suggestion_id) const;

  // Callbacks for the RemoteSuggestionsDatabase.
  void OnDatabaseLoaded(RemoteSuggestion::PtrVector suggestions);
  void OnDatabaseError();

  // Callback for fetch-more requests with the RemoteSuggestionsFetcher.
  void OnFetchMoreFinished(
      FetchDoneCallback fetching_callback,
      Status status,
      RemoteSuggestionsFetcher::OptionalFetchedCategories fetched_categories);

  // Callback for regular fetch requests with the RemoteSuggestionsFetcher.
  void OnFetchFinished(
      FetchStatusCallback callback,
      bool interactive_request,
      Status status,
      RemoteSuggestionsFetcher::OptionalFetchedCategories fetched_categories);

  // Moves all suggestions from |to_archive| into the archive of the |content|.
  // Clears |to_archive|. As the archive is a FIFO buffer of limited size, this
  // function will also delete images from the database in case the associated
  // suggestion gets evicted from the archive.
  void ArchiveSuggestions(CategoryContent* content,
                          RemoteSuggestion::PtrVector* to_archive);

  // Sanitizes newly fetched suggestions -- e.g. adding missing dates and
  // filtering out incomplete results or dismissed suggestions (indicated by
  // |dismissed|).
  void SanitizeReceivedSuggestions(const RemoteSuggestion::PtrVector& dismissed,
                                   RemoteSuggestion::PtrVector* suggestions);

  // Adds newly available suggestions to |content| corresponding to |category|.
  void IntegrateSuggestions(Category category,
                            CategoryContent* content,
                            RemoteSuggestion::PtrVector new_suggestions);

  // Adds newly available suggestion at the top of Articles category.
  void PrependArticleSuggestion(
      std::unique_ptr<RemoteSuggestion> remote_suggestion);

  // Refreshes the content suggestions upon receiving a push-to-refresh request.
  void RefreshSuggestionsUponPushToRefreshRequest();

  // Dismisses a suggestion within a given category content.
  // Note that this modifies the suggestion datastructures of |content|
  // invalidating iterators.
  void DismissSuggestionFromCategoryContent(
      CategoryContent* content,
      const std::string& id_within_category);

  // Sets categories status to NOT_PROVIDED and deletes them (including their
  // suggestions from the database).
  void DeleteCategories(const std::vector<Category>& categories);

  // Removes expired dismissed suggestions from the service and the database.
  void ClearExpiredDismissedSuggestions();

  // Removes images from the DB that are not referenced from any known
  // suggestion. Needs to iterate the whole suggestion database -- so do it
  // often enough to keep it small but not too often as it still iterates over
  // the file system.
  void ClearOrphanedImages();

  // Clears suggestions because any history item has been removed.
  void ClearHistoryDependentState();

  // Clears the cached suggestions
  void ClearCachedSuggestionsImpl();

  // Clears all stored suggestions and updates the observer.
  void NukeAllSuggestions();

  // Completes the initialization phase of the service, registering the last
  // observers. This is done after construction, once the database is loaded.
  void FinishInitialization();

  // Triggers a state transition depending on the provided status. This method
  // is called when a change is detected by |status_service_|.
  void OnStatusChanged(RemoteSuggestionsStatus old_status,
                       RemoteSuggestionsStatus new_status);

  // Verifies state transitions (see |State|'s documentation) and applies them.
  // Also updates the provider status. Does nothing except updating the provider
  // status if called with the current state.
  void EnterState(State state);

  // Notifies the state change to ProviderStatusCallback specified by
  // SetProviderStatusCallback().
  void NotifyStateChanged();

  // Converts the given |suggestions| to content suggestions and notifies the
  // observer with them for category |category|.
  void NotifyNewSuggestions(Category category,
                            const RemoteSuggestion::PtrVector& suggestions);

  // Updates the internal status for |category| to |category_status_| and
  // notifies the content suggestions observer if it changed.
  void UpdateCategoryStatus(Category category, CategoryStatus status);
  // Calls UpdateCategoryStatus() for all provided categories.
  void UpdateAllCategoryStatus(CategoryStatus status);

  // Updates the category info for |category|. If a corresponding
  // CategoryContent object does not exist, it will be created.
  // Returns the existing or newly created object.
  CategoryContent* UpdateCategoryInfo(Category category,
                                      const CategoryInfo& info);

  void RestoreCategoriesFromPrefs();
  void StoreCategoriesToPrefs();

  // If |fetched_category| is nullopt, fetches all categories. Otherwise,
  // fetches at most |count_to_fetch| suggestions only from |fetched_category|.
  // TODO(vitaliii): Also support |count_to_fetch| when |fetched_category| is
  // nullopt.
  RequestParams BuildFetchParams(base::Optional<Category> fetched_category,
                                 int count_to_fetch) const;

  bool AreArticlesEmpty() const;
  bool AreArticlesAvailable() const;
  void NotifyFetchWithLoadingIndicatorStarted();
  void NotifyFetchWithLoadingIndicatorFailedOrTimeouted();

  GURL GetImageURLToFetch(const ContentSuggestion::ID& suggestion_id) const;

  State state_;

  PrefService* pref_service_;

  const Category articles_category_;

  std::map<Category, CategoryContent, Category::CompareByID> category_contents_;

  // The ISO 639-1 code of the language used by the application.
  const std::string application_language_code_;

  // Ranker that orders the categories. Not owned.
  CategoryRanker* category_ranker_;

  // Scheduler to inform about scheduling-related events. Not owned.
  RemoteSuggestionsScheduler* remote_suggestions_scheduler_;

  // The suggestions fetcher.
  std::unique_ptr<RemoteSuggestionsFetcher> suggestions_fetcher_;

  // The database for persisting suggestions.
  std::unique_ptr<RemoteSuggestionsDatabase> database_;
  base::TimeTicks database_load_start_;

  // The image fetcher.
  CachedImageFetcher image_fetcher_;

  // The service that provides events and data about the signin and sync state.
  std::unique_ptr<RemoteSuggestionsStatusService> status_service_;

  // Set to true if ClearHistoryDependentState is called while the service isn't
  // ready. The nuke will be executed once the service finishes initialization
  // or enters the READY state.
  bool clear_history_dependent_state_when_initialized_;

  // Set to true if ClearCachedSuggestions has been called while the service
  // isn't ready. The clearing will be executed once the service finishes
  // initialization or enters the READY state.
  bool clear_cached_suggestions_when_initialized_;

  // A clock for getting the time. This allows to inject a clock in tests.
  base::Clock* clock_;

  // Prefetched pages tracker to query which urls have been prefetched.
  // |nullptr| is handled gracefully and just disables the functionality.
  std::unique_ptr<PrefetchedPagesTracker> prefetched_pages_tracker_;

  // A Timer for canceling too long fetches.
  std::unique_ptr<base::OneShotTimer> fetch_timeout_timer_;

  // Keeps track of the status of the ongoing request(s) and what action should
  // be taken on completion. Requests via Fetch() (fetching more) are _not_
  // tracked by this variable (as they do not need any special actions on
  // completion).
  FetchRequestStatus request_status_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsProviderImpl);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_PROVIDER_IMPL_H_
