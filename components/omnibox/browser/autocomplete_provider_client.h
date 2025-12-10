// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/lens_suggest_inputs_utils.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteClassifier;
class AutocompleteSchemeClassifier;
class AutocompleteScoringModelService;
class DocumentSuggestionsService;
class UnscopedExtensionProvider;
class UnscopedExtensionProviderDelegate;
class GURL;
class InMemoryURLIndex;
class KeywordExtensionsDelegate;
class KeywordProvider;
class OmniboxPedalProvider;
class OmniboxTriggeredFeatureService;
class OnDeviceTailModelService;
class PrefService;
class RemoteSuggestionsService;
class ShortcutsBackend;
class TabMatcher;
class ZeroSuggestCacheService;
struct AutocompleteMatch;
struct ProviderStateService;
class AimEligibilityService;

namespace bookmarks {
class BookmarkModel;
}

namespace history {
class HistoryService;
class TopSites;
class URLDatabase;
}  // namespace history

namespace history_clusters {
class HistoryClustersService;
}

namespace history_embeddings {
class HistoryEmbeddingsService;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace component_updater {
class ComponentUpdateService;
}

namespace signin {
class IdentityManager;
}

namespace tab_groups {
class TabGroupSyncService;
}

class TemplateURLService;

class AutocompleteProviderClient : public OmniboxAction::Client {
 public:
  virtual ~AutocompleteProviderClient() = default;

  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;
  virtual PrefService* GetPrefs() const = 0;
  virtual PrefService* GetLocalState() = 0;
  virtual std::string GetApplicationLocale() const = 0;
  virtual const AutocompleteSchemeClassifier& GetSchemeClassifier() const = 0;
  virtual AutocompleteClassifier* GetAutocompleteClassifier() = 0;
  virtual history::HistoryService* GetHistoryService() = 0;
  virtual history_clusters::HistoryClustersService* GetHistoryClustersService();
  virtual history_embeddings::HistoryEmbeddingsService*
  GetHistoryEmbeddingsService();
  virtual scoped_refptr<history::TopSites> GetTopSites() = 0;
  virtual bookmarks::BookmarkModel* GetBookmarkModel() = 0;
  virtual history::URLDatabase* GetInMemoryDatabase() = 0;
  virtual InMemoryURLIndex* GetInMemoryURLIndex() = 0;
  virtual TemplateURLService* GetTemplateURLService() = 0;
  virtual const TemplateURLService* GetTemplateURLService() const = 0;
  virtual DocumentSuggestionsService* GetDocumentSuggestionsService() const;
  virtual RemoteSuggestionsService* GetRemoteSuggestionsService(
      bool create_if_necessary) const = 0;
  virtual ZeroSuggestCacheService* GetZeroSuggestCacheService() = 0;
  virtual const ZeroSuggestCacheService* GetZeroSuggestCacheService() const = 0;
  virtual OmniboxPedalProvider* GetPedalProvider() const = 0;
  virtual scoped_refptr<ShortcutsBackend> GetShortcutsBackend() = 0;
  virtual scoped_refptr<ShortcutsBackend> GetShortcutsBackendIfExists() = 0;
  virtual std::unique_ptr<KeywordExtensionsDelegate>
  GetKeywordExtensionsDelegate(KeywordProvider* keyword_provider) = 0;
  virtual std::unique_ptr<UnscopedExtensionProviderDelegate>
  GetUnscopedExtensionProviderDelegate(UnscopedExtensionProvider* provider) = 0;
  virtual OmniboxTriggeredFeatureService* GetOmniboxTriggeredFeatureService()
      const = 0;
  virtual AutocompleteScoringModelService* GetAutocompleteScoringModelService()
      const = 0;
  virtual OnDeviceTailModelService* GetOnDeviceTailModelService() const = 0;
  virtual ProviderStateService* GetProviderStateService() const = 0;
  virtual base::CallbackListSubscription GetLensSuggestInputsWhenReady(
      LensOverlaySuggestInputsCallback callback) const = 0;
  virtual tab_groups::TabGroupSyncService* GetTabGroupSyncService() const = 0;
  virtual AimEligibilityService* GetAimEligibilityService() const = 0;

  // The value to use for Accept-Languages HTTP header when making an HTTP
  // request.
  virtual std::string GetAcceptLanguages() const = 0;

  // The embedder's representation of the |about| URL scheme for builtin URLs
  // (e.g., |chrome| for Chrome).
  virtual std::string GetEmbedderRepresentationOfAboutScheme() const = 0;

  // The set of built-in URLs considered worth suggesting as autocomplete
  // suggestions to the user.  Some built-in URLs, e.g. hidden URLs that
  // intentionally crash the product for testing purposes, may be omitted from
  // this list if suggesting them is undesirable.
  virtual std::vector<std::u16string> GetBuiltinURLs() = 0;

  // The set of URLs to provide as autocomplete suggestions as the user types a
  // prefix of the |about| scheme or the embedder's representation of that
  // scheme. Note that this may be a subset of GetBuiltinURLs(), e.g., only the
  // most commonly-used URLs from that set.
  virtual std::vector<std::u16string> GetBuiltinsToProvideAsUserTypes() = 0;

  // TODO(crbug.com/40610979): clean up component update service if it's
  // confirmed it's not needed for on device head provider. The component update
  // service instance which will be used by on device suggestion provider to
  // observe the model update event.
  virtual component_updater::ComponentUpdateService*
  GetComponentUpdateService() = 0;

  // Returns the signin::IdentityManager associated with the current profile.
  virtual signin::IdentityManager* GetIdentityManager() const = 0;
  // In desktop platforms, this returns true for both guest and Incognito mode.
  // In mobile platforms, we don't have a guest mode and therefore, it returns
  // true only for Incognito mode.
  virtual bool IsOffTheRecord() const = 0;
  virtual bool IsIncognitoProfile() const = 0;
  virtual bool IsGuestSession() const = 0;

  virtual bool SearchSuggestEnabled() const = 0;

  // True for almost all users except ones with a specific enterprise policy.
  virtual bool AllowDeletingBrowserHistory() const;

  // Returns whether *anonymized* data collection is enabled.
  // This is used by the client to check whether the user has granted consent
  // for *anonymized* URL-keyed data collection. This currently governs
  // whether we send Suggest requests that include information about the
  // current page URL (when the user has enabled the MSBB opt-in).
  virtual bool IsUrlDataCollectionActive() const = 0;

  // Returns whether *personalized* data collection is enabled.
  // This is used by the client to check whether the user has granted consent
  // for *personalized* URL-keyed data collection keyed by their Google account.
  // This currently governs whether we send Suggest requests that include
  // information about the current page title (when the user has enabled the
  // History Sync opt-in).
  virtual bool IsPersonalizedUrlDataCollectionActive() const = 0;

  // This function returns true if the user is signed in.
  virtual bool IsAuthenticated() const = 0;

  // Determines whether sync is enabled.
  virtual bool IsSyncActive() const = 0;

  virtual std::string ProfileUserName() const;

  // Given some string |text| that the user wants to use for navigation,
  // determines how it should be interpreted.
  virtual void Classify(
      const std::u16string& text,
      bool prefer_keyword,
      bool allow_exact_keyword_match,
      metrics::OmniboxEventProto::PageClassification page_classification,
      AutocompleteMatch* match,
      GURL* alternate_nav_url) = 0;

  // Deletes all URL and search term entries matching the given |term| and
  // |keyword_id| from history.
  virtual void DeleteMatchingURLsForKeywordFromHistory(
      history::KeywordID keyword_id,
      const std::u16string& term) = 0;

  virtual void PrefetchImage(const GURL& url) = 0;

  // Sends a hint to the service worker context that navigation to
  // |destination_url| is likely, unless the current profile is in incognito
  // mode. On platforms where this is supported, the service worker lookup can
  // be expensive so this method should only be called once per input session.
  virtual void StartServiceWorker(const GURL& destination_url) {}

  // Called after creation of |keyword_provider| to allow the client to
  // configure the provider if desired.
  virtual void ConfigureKeywordProvider(KeywordProvider* keyword_provider) {}

  // Called to acquire the instance of TabMatcher, used to identify open tabs
  // for a given set of AutocompleteMatches within the current profile.
  virtual const TabMatcher& GetTabMatcher() const = 0;

  // Returns whether user is currently allowed to enter incognito mode.
  virtual bool IsIncognitoModeAvailable() const;

  // Returns true if the sharing hub command is enabled.
  virtual bool IsSharingHubAvailable() const;

  // Returns true if history embeddings is enabled and user has opted in.
  virtual bool IsHistoryEmbeddingsEnabled() const;

  // Returns true if history embeddings is enabled and user can opt in/out.
  virtual bool IsHistoryEmbeddingsSettingVisible() const;

  // Returns true if the current profile is eligible for Lens. This is used to
  // control whether Lens entrypoints can be shown during this browsing session.
  // Can be changed on demand via enterprise policy.
  virtual bool IsLensEnabled() const;

  // Returns true if the Lens entrypoints can be shown to the user at this
  // instant in time. This is false if Lens is already active, and therefore the
  // entrypoint shouldn't be shown as it will cause nothing to happen if
  // clicked. This is per tab dependent.
  virtual bool AreLensEntrypointsVisible() const;

  // Returns true if the page contains the paywall hint in the HTML. Returns
  // false if the page does not contain the paywall hint. Returns std::nullopt
  // if the page content wasn't extracted and therefore the signal could not be
  // calculated. This is used to control whether contextual suggestions can be
  // shown to the user.
  virtual std::optional<bool> IsPagePaywalled() const;

  // Whether the client should send the `ctxus=` URL parameter to Suggest in
  // order to request contextual search suggestions in the Omnibox.
  virtual bool ShouldSendContextualUrlSuggestParam() const;

  // Whether the client should send the `pageTitle=` URL parameter to Suggest
  // when requesting ZPS suggestions in the Omnibox.
  virtual bool ShouldSendPageTitleSuggestParam() const;

  // Returns whether the app is currently in the background state (Mobile only).
  virtual bool in_background_state() const;

  virtual void set_in_background_state(bool in_background_state) {}

  // Whether the "Omnibox Next" feature param with the given `param_name` is
  // enabled.
  virtual bool IsOmniboxNextFeatureParamEnabled(
      const std::string& param_name) const;

  // Gets a weak pointer to the client. Used when providers need to use the
  // client when the client may no longer be around.
  virtual base::WeakPtr<AutocompleteProviderClient> GetWeakPtr();
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_CLIENT_H_
