// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/ntp_snippets/callbacks.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/status.h"

namespace ntp_snippets {

// Provides content suggestions from one particular source.
// A provider can provide suggestions for multiple ContentSuggestionCategories,
// but for every category that it provides, it will be the only provider in the
// system which provides suggestions for that category.
// Providers are created by the ContentSuggestionsServiceFactory and owned and
// shut down by the ContentSuggestionsService.
class ContentSuggestionsProvider {
 public:
  // The observer of a provider is notified when new data is available.
  class Observer {
   public:
    // Called when the available content changed.
    // If a provider provides suggestions for multiple categories, this callback
    // is called once per category. The |suggestions| parameter always contains
    // the full list of currently available suggestions for that category, i.e.,
    // an empty list will remove all suggestions from the given category. Note
    // that to clear them from the UI immediately, the provider needs to change
    // the status of the respective category. If the given |category| is not
    // known yet, the calling |provider| will be registered as its provider.
    virtual void OnNewSuggestions(
        ContentSuggestionsProvider* provider,
        Category category,
        std::vector<ContentSuggestion> suggestions) = 0;

    // Called when the status of a category changed, including when it is added.
    // If |new_status| is NOT_PROVIDED, the calling provider must be the one
    // that currently provides the |category|, and the category is unregistered
    // without clearing the UI.
    // If |new_status| is any other value, it must match the value that is
    // currently returned from the provider's |GetCategoryStatus(category)|. In
    // case the given |category| is not known yet, the calling |provider| will
    // be registered as its provider. Whenever the status changes to an
    // unavailable status, all suggestions in that category must immediately be
    // removed from all caches and from the UI, but the provider remains
    // registered.
    virtual void OnCategoryStatusChanged(ContentSuggestionsProvider* provider,
                                         Category category,
                                         CategoryStatus new_status) = 0;

    // Called when a suggestion has been invalidated. It will not be provided
    // through |OnNewSuggestions| anymore, is not supported by
    // |FetchSuggestionImage| or |DismissSuggestion| anymore, and should
    // immediately be cleared from the UI and caches. This happens, for example,
    // when the content that the suggestion refers to is gone.
    // Note that this event may be fired even if the corresponding category is
    // not currently AVAILABLE, because open UIs may still be showing the
    // suggestion that is to be removed. This event may also be fired for
    // |suggestion_id|s that never existed and should be ignored in that case.
    virtual void OnSuggestionInvalidated(
        ContentSuggestionsProvider* provider,
        const ContentSuggestion::ID& suggestion_id) = 0;
  };

  virtual ~ContentSuggestionsProvider();

  // Determines the status of the given |category|, see CategoryStatus.
  virtual CategoryStatus GetCategoryStatus(Category category) = 0;

  // Returns the meta information for the given |category|.
  virtual CategoryInfo GetCategoryInfo(Category category) = 0;

  // Dismisses the suggestion with the given ID. A provider needs to ensure that
  // a once-dismissed suggestion is never delivered again (through the
  // Observer). The provider must not call Observer::OnNewSuggestions if the
  // removal of the dismissed suggestion is the only change.
  virtual void DismissSuggestion(
      const ContentSuggestion::ID& suggestion_id) = 0;

  // Fetches the image for the suggestion with the given ID and returns it
  // through the callback. This fetch may occur locally or from the internet.
  // If that suggestion doesn't exist, doesn't have an image or if the fetch
  // fails, the callback gets a null image. The callback will not be called
  // synchronously.
  virtual void FetchSuggestionImage(const ContentSuggestion::ID& suggestion_id,
                                    ImageFetchedCallback callback) = 0;

  // Fetches the image data for the suggestion with the given ID and returns it
  // through the callback. This fetch may occur locally or from the internet.
  // If that suggestion doesn't exist, doesn't have an image or if the fetch
  // fails, the callback gets empty data. The callback will not be called
  // synchronously.
  virtual void FetchSuggestionImageData(
      const ContentSuggestion::ID& suggestion_id,
      ImageDataFetchedCallback callback) = 0;

  // Fetches more suggestions for the given category. The new suggestions
  // will not include any suggestion of the |known_suggestion_ids| sets.
  // As a result of this call, the provider:
  //  - should call the |callback| with these additional suggestions (exactly
  //  once as the front-end might wait for its completion);
  //  - should *not* notify its Observer by OnNewSuggestions() with these
  //  additional suggestions.
  virtual void Fetch(const Category& category,
                     const std::set<std::string>& known_suggestion_ids,
                     FetchDoneCallback callback) = 0;

  // Reloads suggestions from all categories. If the suggestions change, the
  // observer must be notified via OnNewSuggestions();
  // TODO(jkcal): make pure virtual (involves touching all providers) or remove
  // by resolving the pull/push dichotomy.
  virtual void ReloadSuggestions() {}

  // Removes history from the specified time range where the URL matches the
  // |filter|. The data removed depends on the provider. Note that the
  // data outside the time range may be deleted, for example suggestions, which
  // are based on history from that time range. Providers should immediately
  // clear any data related to history from the specified time range where the
  // URL matches the |filter|.
  virtual void ClearHistory(
      base::Time begin,
      base::Time end,
      const base::RepeatingCallback<bool(const GURL& url)>& filter) = 0;

  // Clears suggestions for any non-history related reason (e.g., sign-in status
  // change, etc.) so that the next fetch starts from scratch.
  virtual void ClearCachedSuggestions() = 0;

  // Called when the sign in state has changed. Should be used instead of
  // directly registering with the SignInManager so that the
  // ContentSuggestionService can control the order of the updates between
  // the providers and the observers. |has_signed_in| is true if the state
  // change was due to the user signin in and false if the state change was due
  // to the user signing out.
  virtual void OnSignInStateChanged(bool has_signed_in) {}

  // Used only for debugging purposes. Retrieves suggestions for the given
  // |category| that have previously been dismissed and are still stored in the
  // provider. If the provider doesn't store dismissed suggestions for the given
  // |category|, it always calls the callback with an empty vector. The callback
  // may be called synchronously.
  virtual void GetDismissedSuggestionsForDebugging(
      Category category,
      DismissedSuggestionsCallback callback) = 0;

  // Used only for debugging purposes. Clears the cache of dismissed
  // suggestions for the given |category|, if present, so that no suggestions
  // are suppressed. This does not necessarily make previously dismissed
  // suggestions reappear, as they may have been permanently deleted, depending
  // on the provider implementation.
  virtual void ClearDismissedSuggestionsForDebugging(Category category) = 0;

 protected:
  ContentSuggestionsProvider(Observer* observer);

  Observer* observer() const { return observer_; }

 private:
  Observer* observer_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_PROVIDER_H_
