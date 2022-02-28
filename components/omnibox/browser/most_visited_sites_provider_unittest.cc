// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/most_visited_sites_provider.h"

#include <list>
#include <map>
#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/history/core/browser/top_sites.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace {
class FakeEmptyTopSites : public history::TopSites {
 public:
  // history::TopSites:
  void GetMostVisitedURLs(GetMostVisitedURLsCallback callback) override;
  void SyncWithHistory() override {}
  bool HasBlockedUrls() const override { return false; }
  void AddBlockedUrl(const GURL& url) override {}
  void RemoveBlockedUrl(const GURL& url) override {}
  bool IsBlocked(const GURL& url) override { return false; }
  void ClearBlockedUrls() override {}
  bool IsFull() override { return false; }
  bool loaded() const override { return false; }
  history::PrepopulatedPageList GetPrepopulatedPages() override {
    return history::PrepopulatedPageList();
  }
  void OnNavigationCommitted(const GURL& url) override {}

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override {}

  // Only runs a single callback, so that the test can specify a different
  // set per call.
  void RunACallback(const history::MostVisitedURLList& urls) {
    DCHECK(!callbacks.empty());
    std::move(callbacks.front()).Run(urls);
    callbacks.pop_front();
  }

 protected:
  // A test-specific field for controlling when most visited callback is run
  // after top sites have been requested.
  std::list<GetMostVisitedURLsCallback> callbacks;

  ~FakeEmptyTopSites() override = default;
};

void FakeEmptyTopSites::GetMostVisitedURLs(
    GetMostVisitedURLsCallback callback) {
  callbacks.push_back(std::move(callback));
}

class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient()
      : template_url_service_(new TemplateURLService(nullptr, 0)),
        top_sites_(new FakeEmptyTopSites()) {}
  FakeAutocompleteProviderClient(const FakeAutocompleteProviderClient&) =
      delete;
  FakeAutocompleteProviderClient& operator=(
      const FakeAutocompleteProviderClient&) = delete;

  bool SearchSuggestEnabled() const override { return true; }

  scoped_refptr<history::TopSites> GetTopSites() override { return top_sites_; }

  TemplateURLService* GetTemplateURLService() override {
    return template_url_service_.get();
  }

  TemplateURLService* GetTemplateURLService() const override {
    return template_url_service_.get();
  }

  bool IsPersonalizedUrlDataCollectionActive() const override { return true; }

  void Classify(
      const std::u16string& text,
      bool prefer_keyword,
      bool allow_exact_keyword_match,
      metrics::OmniboxEventProto::PageClassification page_classification,
      AutocompleteMatch* match,
      GURL* alternate_nav_url) override {
    // Populate enough of |match| to keep the MostVisitedSitesProvider happy.
    match->type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
    match->destination_url = GURL(text);
  }

  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override {
    return scheme_classifier_;
  }

 private:
  std::unique_ptr<TemplateURLService> template_url_service_;
  scoped_refptr<history::TopSites> top_sites_;
  TestSchemeClassifier scheme_classifier_;
};

}  // namespace

class MostVisitedSitesProviderTest : public testing::Test,
                                     public AutocompleteProviderListener {
 public:
  MostVisitedSitesProviderTest() = default;
  MostVisitedSitesProviderTest(const MostVisitedSitesProviderTest&) = delete;
  MostVisitedSitesProviderTest& operator=(const MostVisitedSitesProviderTest&) =
      delete;

  void SetUp() override;

 protected:
  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override;

  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<MostVisitedSitesProvider> provider_;

  network::TestURLLoaderFactory* test_loader_factory() {
    return client_->test_url_loader_factory();
  }

  GURL GetSuggestURL(
      metrics::OmniboxEventProto::PageClassification page_classification) {
    TemplateURLRef::SearchTermsArgs search_terms_args;
    search_terms_args.page_classification = page_classification;
    search_terms_args.focus_type = OmniboxFocusType::ON_FOCUS;
    return RemoteSuggestionsService::EndpointUrl(
        search_terms_args, client_->GetTemplateURLService());
  }

  AutocompleteInput CreateNTPOnFocusInputForRemoteNoUrl() {
    // Use NTP as the page classification, since REMOTE_NO_URL is enabled by
    // default for the NTP.
    AutocompleteInput input(
        std::u16string(),
        metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
        TestSchemeClassifier());
    input.set_focus_type(OmniboxFocusType::ON_FOCUS);
    return input;
  }
};

void MostVisitedSitesProviderTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();
  provider_ = new MostVisitedSitesProvider(client_.get(), this);
}

void MostVisitedSitesProviderTest::OnProviderUpdate(bool updated_matches) {}

TEST_F(MostVisitedSitesProviderTest, AllowMostVisitedSitesSuggestions) {
  std::string input_url = "https://example.com/";
  std::string start_surface_url = "chrome://newtab";

  AutocompleteInput prefix_input(base::ASCIIToUTF16(input_url),
                                 metrics::OmniboxEventProto::OTHER,
                                 TestSchemeClassifier());
  prefix_input.set_focus_type(OmniboxFocusType::DEFAULT);

  AutocompleteInput on_focus_input(base::ASCIIToUTF16(input_url),
                                   metrics::OmniboxEventProto::OTHER,
                                   TestSchemeClassifier());
  on_focus_input.set_current_url(GURL(input_url));
  on_focus_input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  AutocompleteInput on_clobber_input(std::u16string(),
                                     metrics::OmniboxEventProto::OTHER,
                                     TestSchemeClassifier());
  on_clobber_input.set_current_url(GURL(input_url));
  on_clobber_input.set_focus_type(OmniboxFocusType::DELETED_PERMANENT_TEXT);

  AutocompleteInput start_surface_input(
      std::u16string(), metrics::OmniboxEventProto::START_SURFACE_HOMEPAGE,
      TestSchemeClassifier());
  start_surface_input.set_current_url(GURL(start_surface_url));
  start_surface_input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  AutocompleteInput start_surface_new_tab_input(
      std::u16string(), metrics::OmniboxEventProto::START_SURFACE_NEW_TAB,
      TestSchemeClassifier());
  start_surface_new_tab_input.set_current_url(GURL());
  start_surface_new_tab_input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  // MostVisited should never deal with prefix suggestions.
  EXPECT_FALSE(provider_->AllowMostVisitedSitesSuggestions(prefix_input));

  // This should always be true, as otherwise we will break MostVisited.
  EXPECT_TRUE(provider_->AllowMostVisitedSitesSuggestions(on_focus_input));

  // Verifies that metrics::OmniboxEventProto::START_SURFACE_HOMEPAGE is allowed
  // for MostVisited.
  EXPECT_TRUE(provider_->AllowMostVisitedSitesSuggestions(start_surface_input));

  // Verifies that metrics::OmniboxEventProto::START_SURFACE_NEW_TAB is allowed
  // for MostVisited.
  EXPECT_TRUE(
      provider_->AllowMostVisitedSitesSuggestions(start_surface_new_tab_input));
}

TEST_F(MostVisitedSitesProviderTest, TestMostVisitedCallback) {
  std::string current_url("http://www.foxnews.com/");
  std::string input_url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(input_url),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(current_url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  history::MostVisitedURLList urls;
  history::MostVisitedURL url(GURL("http://foo.com/"), u"Foo");
  urls.push_back(url);

  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
  scoped_refptr<history::TopSites> top_sites = client_->GetTopSites();
  static_cast<FakeEmptyTopSites*>(top_sites.get())->RunACallback(urls);
  EXPECT_EQ(1U, provider_->matches().size());
  provider_->Stop(false, false);

  provider_->Start(input, false);
  provider_->Stop(false, false);
  EXPECT_TRUE(provider_->matches().empty());
  // Most visited results arriving after Stop() has been called, ensure they
  // are not displayed.
  static_cast<FakeEmptyTopSites*>(top_sites.get())->RunACallback(urls);
  EXPECT_TRUE(provider_->matches().empty());

  history::MostVisitedURLList urls2;
  urls2.push_back(history::MostVisitedURL(GURL("http://bar.com/"), u"Bar"));
  urls2.push_back(history::MostVisitedURL(GURL("http://zinga.com/"), u"Zinga"));
  provider_->Start(input, false);
  provider_->Stop(false, false);
  provider_->Start(input, false);
  static_cast<FakeEmptyTopSites*>(top_sites.get())->RunACallback(urls);
  // Stale results should get rejected.
  EXPECT_TRUE(provider_->matches().empty());
  static_cast<FakeEmptyTopSites*>(top_sites.get())->RunACallback(urls2);
  EXPECT_FALSE(provider_->matches().empty());
  provider_->Stop(false, false);
}

TEST_F(MostVisitedSitesProviderTest, TestMostVisitedNavigateToSearchPage) {
  std::string current_url("http://www.foxnews.com/");
  std::string input_url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(input_url),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(current_url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  history::MostVisitedURLList urls;
  history::MostVisitedURL url(GURL("http://foo.com/"), u"Foo");
  urls.push_back(url);

  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
  // Stop() doesn't always get called.

  std::string search_url("https://www.google.com/?q=flowers");
  AutocompleteInput srp_input(
      base::ASCIIToUTF16(search_url),
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      TestSchemeClassifier());
  srp_input.set_current_url(GURL(search_url));
  srp_input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  provider_->Start(srp_input, false);
  EXPECT_TRUE(provider_->matches().empty());
  // Most visited results arriving after a new request has been started.
  scoped_refptr<history::TopSites> top_sites = client_->GetTopSites();
  static_cast<FakeEmptyTopSites*>(top_sites.get())->RunACallback(urls);
  EXPECT_TRUE(provider_->matches().empty());
}
