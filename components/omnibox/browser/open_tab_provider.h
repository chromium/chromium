// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OPEN_TAB_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OPEN_TAB_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"

// This provider matches user input against open tabs. It is *not* included as a
// default provider.
class OpenTabProvider : public AutocompleteProvider {
 public:
  explicit OpenTabProvider(AutocompleteProviderClient* client);

  OpenTabProvider(const OpenTabProvider&) = delete;
  OpenTabProvider& operator=(const OpenTabProvider&) = delete;

  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~OpenTabProvider() override;

  AutocompleteMatch CreateOpenTabMatch(const AutocompleteInput& input,
                                       const std::u16string& title,
                                       const GURL& url);

  raw_ptr<AutocompleteProviderClient> client_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OPEN_TAB_PROVIDER_H_
