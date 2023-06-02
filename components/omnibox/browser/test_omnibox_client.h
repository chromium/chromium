// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_CLIENT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"

class AutocompleteSchemeClassifier;

// Fake implementation of OmniboxClient for use in tests.
class TestOmniboxClient : public testing::NiceMock<OmniboxClient> {
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
  void SetBookmarkModel(bookmarks::BookmarkModel* bookmark_model);
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  AutocompleteControllerEmitter* GetAutocompleteControllerEmitter() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  bool ShouldDefaultTypedNavigationsToHttps() const override;
  int GetHttpsPortForTesting() const override;
  bool IsUsingFakeHttpsForHttpsUpgradeTesting() const override;
  gfx::Image GetSizedIcon(const gfx::VectorIcon& vector_icon_type,
                          SkColor vector_icon_color) const override;

  MOCK_METHOD(gfx::Image,
              GetFaviconForPageUrl,
              (const GURL& page_url,
               FaviconFetchedCallback on_favicon_fetched));
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
  MOCK_METHOD(LocationBarModel*, GetLocationBarModel, ());

 private:
  SessionID session_id_;
  raw_ptr<bookmarks::BookmarkModel, DanglingUntriaged> bookmark_model_;
  raw_ptr<TemplateURLService, DanglingUntriaged> template_url_service_;
  TestSchemeClassifier scheme_classifier_;
  AutocompleteClassifier autocomplete_classifier_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_CLIENT_H_
