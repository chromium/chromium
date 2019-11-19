// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_MOCK_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_MOCK_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/document_suggestions_service.h"
#include "components/omnibox/browser/omnibox_pedal_provider.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/search_engines/template_url_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

struct AutocompleteMatch;

class MockAutocompleteProviderClient
    : public testing::NiceMock<AutocompleteProviderClient> {
 public:
  MockAutocompleteProviderClient();
  ~MockAutocompleteProviderClient();

  // AutocompleteProviderClient:
  MOCK_METHOD0(GetPrefs, PrefService*());
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
    return template_url_service_.get();
  }
  const TemplateURLService* GetTemplateURLService() const override {
    return template_url_service_.get();
  }
  RemoteSuggestionsService* GetRemoteSuggestionsService(
      bool create_if_necessary) const override {
    return remote_suggestions_service_.get();
  }
  DocumentSuggestionsService* GetDocumentSuggestionsService(
      bool create_if_necessary) const override {
    return document_suggestions_service_.get();
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

  component_updater::ComponentUpdateService* GetComponentUpdateService()
      override {
    return nullptr;
  }

  MOCK_CONST_METHOD0(GetAcceptLanguages, std::string());
  MOCK_CONST_METHOD0(GetEmbedderRepresentationOfAboutScheme, std::string());
  MOCK_METHOD0(GetBuiltinURLs, std::vector<base::string16>());
  MOCK_METHOD0(GetBuiltinsToProvideAsUserTypes, std::vector<base::string16>());
  MOCK_CONST_METHOD0(IsOffTheRecord, bool());
  MOCK_CONST_METHOD0(SearchSuggestEnabled, bool());
  MOCK_CONST_METHOD0(IsPersonalizedUrlDataCollectionActive, bool());
  MOCK_CONST_METHOD0(IsAuthenticated, bool());
  MOCK_CONST_METHOD0(IsSyncActive, bool());

  MOCK_METHOD6(
      Classify,
      void(const base::string16& text,
           bool prefer_keyword,
           bool allow_exact_keyword_match,
           metrics::OmniboxEventProto::PageClassification page_classification,
           AutocompleteMatch* match,
           GURL* alternate_nav_url));
  MOCK_METHOD2(DeleteMatchingURLsForKeywordFromHistory,
               void(history::KeywordID keyword_id, const base::string16& term));
  MOCK_METHOD1(PrefetchImage, void(const GURL& url));

  bool IsTabOpenWithURL(const GURL& url,
                        const AutocompleteInput* input) override {
    return false;
  }

  void set_template_url_service(std::unique_ptr<TemplateURLService> service) {
    template_url_service_ = std::move(service);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  bool IsBrowserUpdateAvailable() const override {
    return browser_update_available_;
  }

  void set_browser_update_available(bool browser_update_available) {
    browser_update_available_ = browser_update_available;
  }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  std::unique_ptr<RemoteSuggestionsService> remote_suggestions_service_;
  std::unique_ptr<DocumentSuggestionsService> document_suggestions_service_;
  std::unique_ptr<OmniboxPedalProvider> pedal_provider_;
  std::unique_ptr<TemplateURLService> template_url_service_;
  bool browser_update_available_;

  DISALLOW_COPY_AND_ASSIGN(MockAutocompleteProviderClient);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_MOCK_AUTOCOMPLETE_PROVIDER_CLIENT_H_
