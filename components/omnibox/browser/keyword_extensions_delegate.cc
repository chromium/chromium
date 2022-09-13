// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/keyword_extensions_delegate.h"

KeywordExtensionsDelegate::KeywordExtensionsDelegate(
    KeywordProvider* provider) {
}

KeywordExtensionsDelegate::~KeywordExtensionsDelegate() {
}

void KeywordExtensionsDelegate::IncrementInputId() {
}

bool KeywordExtensionsDelegate::IsEnabledExtension(
    const std::string& extension_id) {
  return false;
}

bool KeywordExtensionsDelegate::Start(const AutocompleteInput& input,
                                      bool minimal_changes,
                                      const TemplateURL* template_url,
                                      const std::u16string& remaining_input) {
  return false;
}

void KeywordExtensionsDelegate::EnterExtensionKeywordMode(
    const std::string& extension_id) {
}

void KeywordExtensionsDelegate::MaybeEndExtensionKeywordMode() {
}

void KeywordExtensionsDelegate::DeleteSuggestion(
    const TemplateURL* template_url,
    const std::u16string& suggestion_text) {}
