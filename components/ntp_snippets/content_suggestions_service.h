// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_snippets/callbacks.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefService;
class PrefRegistrySimple;

namespace favicon {
class LargeIconService;
}  // namespace favicon

namespace favicon_base {
struct LargeIconImageResult;
}  // namespace favicon_base

namespace ntp_snippets {

class RemoteSuggestionsProvider;

// Retrieves suggestions from a number of ContentSuggestionsProviders and serves
// them grouped into categories. There can be at most one provider per category.
class ContentSuggestionsService : public KeyedService,
                                  public ContentSuggestionsProvider::Observer,
                                  public signin::IdentityManager::Observer,
                                  public history::HistoryServiceObserver {
 public:
  class Observer {
   public:
    // Fired every time the service receives a new set of data for the given
    // |category|, replacing any previously available data (though in most cases
    // there will be an overlap and only a few changes within the data). The new
    // data is then available through |GetSuggestionsForCategory(category)|.
    virtual void OnNewSuggestions(Category category) = 0;

    // Fired when the status of a suggestions category changed. Note that for
    // some status changes, the UI must update immediately (e.g. to remove
    // invalidated suggestions). See comments on the individual CategoryStatus
    // values for details.
    virtual void OnCategoryStatusChanged(Category category,
                                         CategoryStatus new_status) = 0;

    // Fired when a suggestion has been invalidated. The UI must immediately
    // clear the suggestion even from open NTPs. Invalidation happens, for
    // example, when the content that the suggestion refers to is gone.
    // Note that this event may be fired even if the corresponding category is
    // not currently AVAILABLE, because open UIs may still be showing the
    // suggestion that is to be removed. This event may also be fired for
    // |suggestion_id|s that never existed and should be ignored in that case.
    virtual void OnSuggestionInvalidated(
        const ContentSuggestion::ID& suggestion_id) = 0;

    // Fired when the previously sent data is not valid anymore and a refresh
    // of all the suggestions is required. Called for example when the sign in
    // state changes and personalised suggestions have to be shown or discarded.
    virtual void OnFullRefreshRequired() = 0;

    // Sent when the service is shutting down. After the service has shut down,
    // it will not provide any data anymore, though calling the getters is still
    // safe.
    virtual void ContentSuggestionsServiceShutdown() = 0;

   protected:
    virtual ~Observer() = default;
  };

  enum class State {
    ENABLED,
    DISABLED,
  };

  ContentSuggestionsService(
      State state,
      signin::IdentityManager*
          identity_manager,                      // Can be nullptr in unittests.
      history::HistoryService* history_service,  // Can be nullptr in unittests.
      // Can be nullptr in unittests.
      favicon::LargeIconService* large_icon_service,
      PrefService* pref_service,
      std::unique_ptr<CategoryRanker> category_ranker,
      std::unique_ptr<UserClassifier> user_classifier,
      std::unique_ptr<RemoteSuggestionsScheduler>
          remote_suggestions_scheduler);  // Can be nullptr in unittests.
  ~ContentSuggestionsService() override;

  // Inherited from KeyedService.
  void Shutdown() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  State state() { return state_; }

  // Gets all categories for which a provider is registered. The categories may
  // or may not be available, see |GetCategoryStatus()|. The order in which the
  // categories are returned is the order in which they should be displayed.
  std::vector<Category> GetCategories() const;

  // Gets the status of a category.
  CategoryStatus GetCategoryStatus(Category category) const;

  // Gets the meta information of a category.
  base::Optional<CategoryInfo> GetCategoryInfo(Category category) const;

  // Gets the available suggestions for a category. The result is empty if the
  // category is available and empty, but also if the category is unavailable
  // for any reason, see |GetCategoryStatus()|.
  const std::vector<ContentSuggestion>& GetSuggestionsForCategory(
      Category category) const;

  // Fetches the image for the suggestion with the given |suggestion_id| and
  // runs the |callback|. If that suggestion doesn't exist or the fetch fails,
  // the callback gets an empty image. The callback will not be called
  // synchronously.
  void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                            ImageFetchedCallback callback);

  // Fetches the image data for the suggestion with the given |suggestion_id|
  // and runs the |callback|. If that suggestion doesn't exist or the fetch
  // fails, the callback gets empty data. The callback will not be called
  // synchronously.
  void FetchSuggestionImageData(const ContentSuggestion::ID& suggestion_id,
                                ImageDataFetchedCallback callback);

  // Fetches the favicon from local cache (if larger than or equal to
  // |minimum_size_in_pixel|) or from Google server (if there is no icon in the
  // cache) and returns the results in the callback. If that suggestion doesn't
  // exist or the fetch fails, the callback gets an empty image. The callback
  // will not be called synchronously.
  void FetchSuggestionFavicon(const ContentSuggestion::ID& suggestion_id,
                              int minimum_size_in_pixel,
                              int desired_size_in_pixel,
                              ImageFetchedCallback callback);

  // Dismisses the suggestion with the given |suggestion_id|, if it exists.
  // This will not trigger an update through the observers (i.e. providers must
  // not call |Observer::OnNewSuggestions|).
  void DismissSuggestion(const ContentSuggestion::ID& suggestion_id);

  // Dismisses the given |category|, if it exists.
  // This will not trigger an update through the observers.
  void DismissCategory(Category category);

  // Restores all dismissed categories.
  // This will not trigger an update through the observers.
  void RestoreDismissedCategories();

  // Returns whether |category| is dismissed.
  bool IsCategoryDismissed(Category category) const;

  // Fetches additional contents for the given |category|. If the fetch was
  // completed, the given |callback| is called with the updated content.
  // This includes new and old data.
  // TODO(jkrcal): Consider either renaming this to FetchMore or unify the ways
  // to get suggestions to just this async Fetch() API.
  void Fetch(const Category& category,
             const std::set<std::string>& known_suggestion_ids,
             FetchDoneCallback callback);

  // Reloads suggestions from all categories, from all providers. If a provider
  // naturally has some ability to generate fresh suggestions, it may provide a
  // completely new set of suggestions. If the provider has no ability to
  // generate fresh suggestions on demand, it may only fill in any vacant space
  // by suggestions that were previously not included due to space limits (there
  // may be vacant space because of the user dismissing suggestions in the
  // meantime).
  void ReloadSuggestions();

  // Observer accessors.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Registers a new ContentSuggestionsProvider. It must be ensured that at most
  // one provider is registered for every category and that this method is
  // called only once per provider.
  void RegisterProvider(std::unique_ptr<ContentSuggestionsProvider> provider);

  // Removes history from the specified time range where the URL matches the
  // |filter| from all providers. The data removed depends on the provider. Note
  // that the data outside the time range may be deleted, for example
  // suggestions, which are based on history from that time range. Providers
  // should immediately clear any data related to history from the specified
  // time range where the URL matches the |filter|.
  void ClearHistory(base::Time begin,
                    base::Time end,
                    const base::Callback<bool(const GURL& url)>& filter);

  // Removes all suggestions from all caches or internal stores in all
  // providers. It does, however, not remove any suggestions from the provider's
  // sources, so if its configuration hasn't changed, it might return the same
  // results when it fetches the next time. In particular, calling this method
  // will not mark any suggestions as dismissed.
  void ClearAllCachedSuggestions();

  // Only for debugging use through the internals page.
  // Retrieves suggestions of the given |category| that have previously been
  // dismissed and are still stored in the respective provider. If the
  // provider doesn't store dismissed suggestions, the callback receives an
  // empty vector. The callback may be called synchronously.
  void GetDismissedSuggestionsForDebugging(
      Category category,
      DismissedSuggestionsCallback callback);

  // Only for debugging use through the internals page. Some providers
  // internally store a list of dismissed suggestions to prevent them from
  // reappearing. This function clears all suggestions of the given |category|
  // from such lists, making dismissed suggestions reappear (if the provider
  // supports it).
  void ClearDismissedSuggestionsForDebugging(Category category);

  // Returns true if the remote suggestions provider is enabled.
  bool AreRemoteSuggestionsEnabled() const;

  // The reference to the RemoteSuggestionsProvider provider should
  // only be set by the factory and only used for debugging.
  // TODO(jkrcal) The way we deal with the circular dependency feels wrong.
  // Consider swapping the dependencies: first constructing all providers, then
  // constructing the service (passing the remote provider as arg), finally
  // registering the service as an observer of all providers?
  // TODO(jkrcal) Move the getter into the scheduler interface (the setter is
  // then not needed any more). crbug.com/695447
  void set_remote_suggestions_provider(
      RemoteSuggestionsProvider* remote_suggestions_provider) {
    remote_suggestions_provider_ = remote_suggestions_provider;
  }
  RemoteSuggestionsProvider* remote_suggestions_provider_for_debugging() {
    return remote_suggestions_provider_;
  }

  // The interface is suited for informing about external events that have
  // influence on scheduling remote fetches. Can be nullptr in tests.
  RemoteSuggestionsScheduler* remote_suggestions_scheduler() {
    return remote_suggestions_scheduler_.get();
  }

  // Can be nullptr in tests.
  // TODO(jkrcal): The getter is only used from the bridge and from
  // snippets-internals. Can we get rid of it with the metrics refactoring?
  UserClassifier* user_classifier() { return user_classifier_.get(); }

  CategoryRanker* category_ranker() { return category_ranker_.get(); }

 private:
  friend class ContentSuggestionsServiceTest;

  // Implementation of ContentSuggestionsProvider::Observer.
  void OnNewSuggestions(ContentSuggestionsProvider* provider,
                        Category category,
                        std::vector<ContentSuggestion> suggestions) override;
  void OnCategoryStatusChanged(ContentSuggestionsProvider* provider,
                               Category category,
                               CategoryStatus new_status) override;
  void OnSuggestionInvalidated(
      ContentSuggestionsProvider* provider,
      const ContentSuggestion::ID& suggestion_id) override;

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(const CoreAccountInfo& account_info) override;
  void OnPrimaryAccountCleared(const CoreAccountInfo& account_info) override;

  // history::HistoryServiceObserver implementation.
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Registers the given |provider| for the given |category|, unless it is
  // already registered. Returns true if the category was newly registered or
  // false if it is dismissed or was present before.
  bool TryRegisterProviderForCategory(ContentSuggestionsProvider* provider,
                                      Category category);
  void RegisterCategory(Category category,
                        ContentSuggestionsProvider* provider);
  void UnregisterCategory(Category category,
                          ContentSuggestionsProvider* provider);

  // Removes a suggestion from the local store |suggestions_by_category_|, if it
  // exists. Returns true if a suggestion was removed.
  bool RemoveSuggestionByID(const ContentSuggestion::ID& suggestion_id);

  // Fires the OnCategoryStatusChanged event for the given |category|.
  void NotifyCategoryStatusChanged(Category category);

  void OnSignInStateChanged(bool has_signed_in);

  // Re-enables a dismissed category, making querying its provider possible.
  void RestoreDismissedCategory(Category category);

  void RestoreDismissedCategoriesFromPrefs();
  void StoreDismissedCategoriesToPrefs();

  // Not implemented for articles. For all other categories, destroys its
  // provider, deletes all mentions (except from dismissed list) and notifies
  // observers that the category is disabled.
  void DestroyCategoryAndItsProvider(Category category);

  // Get the domain of the suggestion suitable for fetching the favicon.
  GURL GetFaviconDomain(const ContentSuggestion::ID& suggestion_id);

  // Initiate the fetch of a favicon from the local cache.
  void GetFaviconFromCache(const GURL& publisher_url,
                           int minimum_size_in_pixel,
                           int desired_size_in_pixel,
                           ImageFetchedCallback callback,
                           bool continue_to_google_server);

  // Callbacks for fetching favicons.
  void OnGetFaviconFromCacheFinished(
      const GURL& publisher_url,
      int minimum_size_in_pixel,
      int desired_size_in_pixel,
      ImageFetchedCallback callback,
      bool continue_to_google_server,
      const favicon_base::LargeIconImageResult& result);
  void OnGetFaviconFromGoogleServerFinished(
      const GURL& publisher_url,
      int minimum_size_in_pixel,
      int desired_size_in_pixel,
      ImageFetchedCallback callback,
      favicon_base::GoogleFaviconServerRequestStatus status);

  // Whether the content suggestions feature is enabled.
  State state_;

  // All registered providers, owned by the service.
  std::vector<std::unique_ptr<ContentSuggestionsProvider>> providers_;

  // All registered categories and their providers. A provider may be contained
  // multiple times, if it provides multiple categories. The keys of this map
  // are exactly the entries of |categories_| and the values are a subset of
  // |providers_|.
  std::map<Category, ContentSuggestionsProvider*, Category::CompareByID>
      providers_by_category_;

  // All dismissed categories and their providers. These may be restored by
  // RestoreDismissedCategories(). The provider can be null if the dismissed
  // category has received no updates since initialisation.
  // (see RestoreDismissedCategoriesFromPrefs())
  std::map<Category, ContentSuggestionsProvider*, Category::CompareByID>
      dismissed_providers_by_category_;

  // All current suggestion categories in arbitrary order. This vector contains
  // exactly the same categories as |providers_by_category_|.
  std::vector<Category> categories_;

  // All current suggestions grouped by category. This contains an entry for
  // every category in |categories_| whose status is an available status. It may
  // contain an empty vector if the category is available but empty (or still
  // loading).
  std::map<Category, std::vector<ContentSuggestion>, Category::CompareByID>
      suggestions_by_category_;

  // Observer for the IdentityManager. All observers are notified when the
  // signin state changes so that they can refresh their list of suggestions.
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_;

  // Observer for the HistoryService. All providers are notified when history is
  // deleted.
  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;

  base::ObserverList<Observer>::Unchecked observers_;

  const std::vector<ContentSuggestion> no_suggestions_;

  base::CancelableTaskTracker favicons_task_tracker_;

  // Keep a direct reference to this special provider to redirect debugging
  // calls to it. If the RemoteSuggestionsProvider is loaded, it is also present
  // in |providers_|, otherwise this is a nullptr.
  RemoteSuggestionsProvider* remote_suggestions_provider_;

  favicon::LargeIconService* large_icon_service_;

  PrefService* pref_service_;

  // Interface for informing about external events that have influence on
  // scheduling remote fetches.
  std::unique_ptr<RemoteSuggestionsScheduler> remote_suggestions_scheduler_;

  // Classifies the user on the basis of long-term user interactions.
  std::unique_ptr<UserClassifier> user_classifier_;

  // Provides order for categories.
  std::unique_ptr<CategoryRanker> category_ranker_;

  DISALLOW_COPY_AND_ASSIGN(ContentSuggestionsService);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SERVICE_H_
