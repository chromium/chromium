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

class AutocompleteSchemeClassifier;

// Fake implementation of OmniboxClient for use in tests.
class TestOmniboxClient : public OmniboxClient {
 public:
  TestOmniboxClient();
  ~TestOmniboxClient() override;
  TestOmniboxClient(const TestOmniboxClient&) = delete;
  TestOmniboxClient& operator=(const TestOmniboxClient&) = delete;

  // OmniboxClient:
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  bool IsPasteAndGoEnabled() const override;
  const SessionID& GetSessionID() const override;
  void SetBookmarkModel(bookmarks::BookmarkModel* bookmark_model);
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  bool ShouldDefaultTypedNavigationsToHttps() const override;
  int GetHttpsPortForTesting() const override;
  bool IsUsingFakeHttpsForHttpsUpgradeTesting() const override;
  gfx::Image GetSizedIcon(const gfx::VectorIcon& vector_icon_type,
                          SkColor vector_icon_color) const override;
  gfx::Image GetFaviconForPageUrl(
      const GURL& page_url,
      FaviconFetchedCallback on_favicon_fetched) override;

  GURL GetPageUrlForLastFaviconRequest() const;

 private:
  SessionID session_id_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<TemplateURLService> template_url_service_;
  TestSchemeClassifier scheme_classifier_;
  AutocompleteClassifier autocomplete_classifier_;
  GURL page_url_for_last_favicon_request_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_CLIENT_H_
