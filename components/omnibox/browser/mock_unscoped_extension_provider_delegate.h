// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_MOCK_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_H_
#define COMPONENTS_OMNIBOX_BROWSER_MOCK_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_H_

#include "components/omnibox/browser/unscoped_extension_provider_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockUnscopedExtensionProviderDelegate
    : public UnscopedExtensionProviderDelegate {
 public:
  MockUnscopedExtensionProviderDelegate();
  MockUnscopedExtensionProviderDelegate(
      const MockUnscopedExtensionProviderDelegate&) = delete;
  MockUnscopedExtensionProviderDelegate& operator=(
      const MockUnscopedExtensionProviderDelegate&) = delete;
  ~MockUnscopedExtensionProviderDelegate() override;

  MOCK_METHOD(void,
              Start,
              (const AutocompleteInput&, bool, std::set<std::string>),
              (override));
  MOCK_METHOD(void, Stop, (bool clear_cached_suggestions), (override));
  MOCK_METHOD(void,
              DeleteSuggestion,
              (const TemplateURL*, const std::u16string&),
              (override));
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_MOCK_UNSCOPED_EXTENSION_PROVIDER_DELEGATE_H_
