// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FEATURED_SEARCH_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_FEATURED_SEARCH_PROVIDER_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteInput;
class AutocompleteProviderClient;
class TemplateURLService;

// This is the provider for built-in URLs, such as about:settings and
// chrome://version, as well as the built-in Starter Pack search engines.
class FeaturedSearchProvider : public AutocompleteProvider {
 public:
  explicit FeaturedSearchProvider(AutocompleteProviderClient* client);
  FeaturedSearchProvider(const FeaturedSearchProvider&) = delete;
  FeaturedSearchProvider& operator=(const FeaturedSearchProvider&) = delete;

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~FeaturedSearchProvider() override;

  static const int kRelevance;
  static const int kStarterPackRelevance;

  // Populates `matches_` with matching starter pack keywords such as @history,
  // and @bookmarks
  void DoStarterPackAutocompletion(const AutocompleteInput& input);

  // Constructs an AutocompleteMatch for starter pack suggestions such as
  // @bookmarks, @history, etc. and adds it to `matches_`.
  void AddStarterPackMatch(const TemplateURL& template_url,
                           const AutocompleteInput& input);

  raw_ptr<AutocompleteProviderClient> client_;
  raw_ptr<TemplateURLService> template_url_service_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FEATURED_SEARCH_PROVIDER_H_
