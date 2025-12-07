// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_H_
#define COMPONENTS_OMNIBOX_BROWSER_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_H_

#include <set>
#include <string>

#include "components/omnibox/browser/autocomplete_input.h"

class UnscopedExtensionProviderDelegate {
 public:
  UnscopedExtensionProviderDelegate();
  UnscopedExtensionProviderDelegate(const UnscopedExtensionProviderDelegate&) =
      delete;
  UnscopedExtensionProviderDelegate& operator=(
      const UnscopedExtensionProviderDelegate&) = delete;
  virtual ~UnscopedExtensionProviderDelegate();

  // Starts a new request to the extension.
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes,
                     std::set<std::string> unscoped_mode_extension_ids) = 0;

  // Stops the current request to the extension by incrementing the current
  // request ID which effectively discards any suggestions that may be incoming
  // later with a stale request ID. if `clear_cached_results` is true, it also
  // clears the current list of cached matches and suggestion group information.
  virtual void Stop(bool clear_cached_results) = 0;

  // Called when the user asks to delete a match an extension previously marked
  // deletable.
  virtual void DeleteSuggestion(const TemplateURL* template_url,
                                const std::u16string& suggestion_text) = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_H_
