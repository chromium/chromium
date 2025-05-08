// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_UNSCOPED_EXTENSION_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_UNSCOPED_EXTENSION_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_suggestions_watcher.h"
#include "third_party/omnibox_proto/groups.pb.h"

class AutocompleteInput;
class AutocompleteProviderClient;
class AutocompleteProviderListener;
class TemplateURLService;
class UnscopedExtensionProviderDelegate;

// Provides suggestions from extension that are allowed to run in unscoped mode
// (i.e. without requiring keyword mode).
class UnscopedExtensionProvider : public AutocompleteProvider {
 public:
  UnscopedExtensionProvider(AutocompleteProviderClient* client,
                            AutocompleteProviderListener* listener);
  UnscopedExtensionProvider(const UnscopedExtensionProvider&) = delete;
  UnscopedExtensionProvider& operator=(const UnscopedExtensionProvider&) =
      delete;

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(AutocompleteStopReason stop_reason) override;
  void DeleteMatch(const AutocompleteMatch& match) override;

  // Used by UnscopedExtensionProviderDelegateImpl.
  TemplateURLService* GetTemplateURLService() const;
  void AddToSuggestionGroupsMap(omnibox::GroupId group_id,
                                omnibox::GroupConfig group_config);
  void set_done(bool done) { done_ = done; }
  ACMatches* matches() { return &matches_; }

 private:
  ~UnscopedExtensionProvider() override;

  raw_ptr<AutocompleteProviderClient> client_;
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<UnscopedExtensionProviderDelegate> delegate_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_UNSCOPED_EXTENSION_PROVIDER_H_
