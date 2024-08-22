// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_MOCK_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_MOCK_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/history/core/browser/top_sites.h"
#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/document_suggestions_service.h"
#include "components/omnibox/browser/keyword_extensions_delegate.h"
#include "components/omnibox/browser/mock_tab_matcher.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

class AutocompleteScoringModelService;
class OnDeviceTailModelService;
class OmniboxTriggeredFeatureService;

struct AutocompleteMatch;
struct ProviderStateService;

class MockAutocompleteProviderClient
    : public testing::NiceMock<AutocompleteProviderClient> {
 public:
  MockAutocompleteProviderClient();
  ~MockAutocompleteProviderClient();
  MockAutocompleteProviderClient(const MockAutocompleteProviderClient&) =
      delete;
  MockAutocompleteProviderClient& operator=(
      const MockAutocompleteProviderClient&) = delete;

  // AutocompleteProviderClient:
  MOCK_CONST_METHOD0(GetPrefs, PrefService*());
  MOCK_METHOD0(GetLocalState, PrefService*());
  MOCK_CONST_METHOD0(GetApplicationLocale, std::string());
  MOCK_CONST_METHOD0(GetSchemeClassifier,
                     const AutocompleteSchemeClassifier&());
  MOCK_METHOD0(GetAutocompleteClassifier, AutocompleteClassifier*());
  MOCK_METHOD0(GetHistoryService, history::HistoryService*());

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return shared_factory_;
  }
  scoped_refptr<history::TopSites> GetTopSites() override { return nullptr; }

  MOCK_METHOD0(GetBookmarkModel, bookmarks::BookmarkModel*());
  MOCK_METHOD0(GetInMemoryDatabase, history::URLDatabase*());
  MOCK_METHOD0(GetInMemoryURLIndex, InMemoryURLIndex*());

  TemplateURLService* GetTemplateURLService() override {
    return template_url_service_;
  }
  const TemplateURLService* GetTemplateURLService() const override {
    return template_url_service_;
  }
  RemoteSuggestionsService* GetRemoteSuggestionsService(
      bool create_if_necessary) const override {
    return remote_suggestions_service_.get();
  }
  ZeroSuggestCacheService* GetZeroSuggestCacheService() override {
    return zero_suggest_cache_service_.get();
  }
  const ZeroSuggestCacheService* GetZeroSuggestCacheService() const override {
    return zero_suggest_cache_service_.get();
  }
  OmniboxPedalProvider* GetPedalProvider() const override {
    return pedal_provider_.get();
  }

  // Can't mock scoped_refptr :\.
  scoped_refptr<ShortcutsBackend> GetShortcutsBackend() override {
    return nullptr;
  }
  scoped_refptr<ShortcutsBackend> GetShortcutsBackendIfExists() override {
    return nullptr;
  }
  std::unique_ptr<KeywordExtensionsDelegate> GetKeywordExtensionsDelegate(
      KeywordProvider* keyword_provider) override {
    return nullptr;
  }
  OmniboxTriggeredFeatureService* GetOmniboxTriggeredFeatureService()
      const override {
    return omnibox_triggered_feature_service_.get();
  }
  component_updater::ComponentUpdateService* GetComponentUpdateService()
      override {
    return nullptr;
  }
  const TabMatcher& GetTabMatcher() const override { return tab_matcher_; }

  signin::IdentityManager* GetIdentityManager() const override {
    return identity_manager_;
  }

  AutocompleteScoringModelService* GetAutocompleteScoringModelService()
      const override {
    return nullptr;
  }

  OnDeviceTailModelService* GetOnDeviceTailModelService() const override {
    return nullptr;
  }

  ProviderStateService* GetProviderStateService() const override {
    return provider_state_service_.get();
  }

  bool in_background_state() const override { return in_background_state_; }

  void set_in_background_state(bool in_background_state) override {
    in_background_state_ = in_background_state;
  }

  MOCK_CONST_METHOD0(GetAcceptLanguages, std::string());
  MOCK_CONST_METHOD0(GetEmbedderRepresentationOfAboutScheme, std::string());
  MOCK_METHOD0(GetBuiltinURLs, std::vector<std::u16string>());
  MOCK_METHOD0(GetBuiltinsToProvideAsUserTypes, std::vector<std::u16string>());
  MOCK_CONST_METHOD0(IsOffTheRecord, bool());
  MOCK_CONST_METHOD0(IsIncognitoProfile, bool());
  MOCK_CONST_METHOD0(IsGuestSession, bool());
  MOCK_CONST_METHOD0(SearchSuggestEnabled, bool());
  MOCK_CONST_METHOD0(IsPersonalizedUrlDataCollectionActive, bool());
  MOCK_CONST_METHOD0(IsAuthenticated, bool());
  MOCK_CONST_METHOD0(IsSyncActive, bool());
  MOCK_CONST_METHOD0(IsHistoryEmbeddingsEnabled, bool());
  MOCK_CONST_METHOD0(IsHistoryEmbeddingsSettingVisible, bool());

  MOCK_METHOD6(
      Classify,
      void(const std::u16string& text,
           bool prefer_keyword,
           bool allow_exact_keyword_match,
           metrics::OmniboxEventProto::PageClassification page_classification,
           AutocompleteMatch* match,
           GURL* alternate_nav_url));
  MOCK_METHOD2(DeleteMatchingURLsForKeywordFromHistory,
               void(history::KeywordID keyword_id, const std::u16string& term));
  MOCK_METHOD1(PrefetchImage, void(const GURL& url));

  void set_pedal_provider(
      std::unique_ptr<OmniboxPedalProvider> pedal_provider) {
    pedal_provider_ = std::move(pedal_provider);
  }

  void set_template_url_service(TemplateURLService* template_url_service) {
    template_url_service_ = template_url_service;
  }

  void set_identity_manager(signin::IdentityManager* identity_manager) {
    identity_manager_ = identity_manager;
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  MOCK_METHOD0(OpenSharingHub, void());
  MOCK_METHOD0(NewIncognitoWindow, void());
  MOCK_METHOD0(OpenIncognitoClearBrowsingDataDialog, void());
  MOCK_METHOD0(CloseIncognitoWindows, void());
  MOCK_METHOD0(PromptPageTranslation, void());

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  bool in_background_state_ = false;

  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<DocumentSuggestionsService> document_suggestions_service_;
  std::unique_ptr<RemoteSuggestionsService> remote_suggestions_service_;
  std::unique_ptr<ZeroSuggestCacheService> zero_suggest_cache_service_;
  std::unique_ptr<OmniboxPedalProvider> pedal_provider_;
  std::unique_ptr<OmniboxTriggeredFeatureService>
      omnibox_triggered_feature_service_;
  std::unique_ptr<ProviderStateService> provider_state_service_;
  MockTabMatcher tab_matcher_;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;  // Not owned.
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_MOCK_AUTOCOMPLETE_PROVIDER_CLIENT_H_
