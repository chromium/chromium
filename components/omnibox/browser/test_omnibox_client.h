// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_CLIENT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image.h"

class AutocompleteSchemeClassifier;

// Fake implementation of OmniboxClient for use in tests.
class TestOmniboxClient final : public testing::NiceMock<OmniboxClient> {
 public:
  TestOmniboxClient();
  ~TestOmniboxClient() override;
  TestOmniboxClient(const TestOmniboxClient&) = delete;
  TestOmniboxClient& operator=(const TestOmniboxClient&) = delete;

  // OmniboxClient:
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  bool IsPasteAndGoEnabled() const override;
  SessionID GetSessionID() const override;
  AutocompleteControllerEmitter* GetAutocompleteControllerEmitter() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  bool ShouldDefaultTypedNavigationsToHttps() const override;
  int GetHttpsPortForTesting() const override;
  bool IsUsingFakeHttpsForHttpsUpgradeTesting() const override;
  gfx::Image GetSizedIcon(const gfx::VectorIcon& vector_icon_type,
                          SkColor vector_icon_color) const override;
  std::u16string GetFormattedFullURL() const override;
  std::u16string GetURLForDisplay() const override;
  GURL GetNavigationEntryURL() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  net::CertStatus GetCertStatus() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  void OnURLOpenedFromOmnibox(OmniboxLog* log) override;

  MOCK_METHOD(gfx::Image,
              GetFaviconForPageUrl,
              (const GURL& page_url,
               FaviconFetchedCallback on_favicon_fetched));
  MOCK_METHOD(void,
              ShowFeedbackPage,
              (const std::u16string& input_text, const GURL& destination_url));
  MOCK_METHOD(void,
              OnAutocompleteAccept,
              (const GURL& destination_url,
               TemplateURLRef::PostContent* post_content,
               WindowOpenDisposition disposition,
               ui::PageTransition transition,
               AutocompleteMatchType::Type match_type,
               base::TimeTicks match_selection_timestamp,
               bool destination_url_entered_without_scheme,
               bool destination_url_entered_with_http_scheme,
               const std::u16string& text,
               const AutocompleteMatch& match,
               const AutocompleteMatch& alternative_nav_match,
               IDNA2008DeviationCharacter deviation_char_in_hostname));
  MOCK_METHOD(bookmarks::BookmarkModel*, GetBookmarkModel, ());
  MOCK_METHOD(PrefService*, GetPrefs, (), (override));
  MOCK_METHOD(const PrefService*, GetPrefs, (), (const, override));

  base::WeakPtr<OmniboxClient> AsWeakPtr() override;

  WindowOpenDisposition last_log_disposition() const {
    return last_log_disposition_;
  }

  TestLocationBarModel* location_bar_model() { return &location_bar_model_; }

 private:
  SessionID session_id_;
  TestLocationBarModel location_bar_model_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  TestSchemeClassifier scheme_classifier_;
  AutocompleteClassifier autocomplete_classifier_;
  WindowOpenDisposition last_log_disposition_;
  base::WeakPtrFactory<TestOmniboxClient> weak_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_CLIENT_H_
