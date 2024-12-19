// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_H_
#define COMPONENTS_OMNIBOX_BROWSER_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_H_

#include <string>

#include "components/omnibox/browser/autocomplete_input.h"

class AutocompleteInput;

class UnscopedExtensionProviderDelegate {
 public:
  UnscopedExtensionProviderDelegate();
  UnscopedExtensionProviderDelegate(const UnscopedExtensionProviderDelegate&) =
      delete;
  UnscopedExtensionProviderDelegate& operator=(
      const UnscopedExtensionProviderDelegate&) = delete;
  virtual ~UnscopedExtensionProviderDelegate();

  // Starts a new request to the extension.
  virtual bool Start(const AutocompleteInput& input,
                     bool minimal_changes,
                     std::set<std::string> unscoped_mode_extension_ids) = 0;

  // Increments the id of the request sent to the extension.
  virtual void IncrementRequestId() = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_H_
