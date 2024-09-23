// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OPEN_TAB_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OPEN_TAB_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/search_engines/template_url.h"

// This provider matches user input against open tabs. It is *not* included as a
// default provider.
class OpenTabProvider : public AutocompleteProvider {
 public:
  explicit OpenTabProvider(AutocompleteProviderClient* client);

  OpenTabProvider(const OpenTabProvider&) = delete;
  OpenTabProvider& operator=(const OpenTabProvider&) = delete;

  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  friend class OpenTabProviderTest;
  ~OpenTabProvider() override;

  AutocompleteMatch CreateOpenTabMatch(const AutocompleteInput& input,
                                       const std::u16string& title,
                                       const GURL& url,
                                       int score,
                                       const TemplateURL* template_url);

  // This is called when no other matches were found and generates a
  // NULL_RESULT_MESSAGE match. This match is intended only to display a message
  // to the user and keep the keyword mode UI.  No action can be taken by
  // opening this match.
  AutocompleteMatch CreateNullResultMessageMatch(
      const AutocompleteInput& input,
      const TemplateURL* template_url);

  raw_ptr<AutocompleteProviderClient> client_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OPEN_TAB_PROVIDER_H_
