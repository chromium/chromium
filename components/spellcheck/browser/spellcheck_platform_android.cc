// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_platform.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "components/spellcheck/common/spellcheck_features.h"

namespace spellcheck_platform {

void GetAvailableLanguages(std::vector<std::string>* spellcheck_languages) {
}

std::string GetSpellCheckerLanguage() {
  return std::string();
}

bool SpellCheckerAvailable() {
  return spellcheck::IsAndroidSpellCheckFeatureEnabled();
}

bool SpellCheckerProvidesPanel() {
  return false;
}

bool SpellingPanelVisible() {
  return false;
}

void ShowSpellingPanel(bool show) {
}

void UpdateSpellingPanelWithMisspelledWord(const base::string16& word) {
}

bool PlatformSupportsLanguage(const std::string& current_language) {
  return true;
}

void SetLanguage(const std::string& lang_to_set,
                 base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

void DisableLanguage(const std::string& lang_to_disable) {}

bool CheckSpelling(const base::string16& word_to_check, int tag) {
  return true;
}

void FillSuggestionList(const base::string16& wrong_word,
                        std::vector<base::string16>* optional_suggestions) {
}

void AddWord(const base::string16& word) {
}

void RemoveWord(const base::string16& word) {
}

int GetDocumentTag() {
  return 1;
}

void IgnoreWord(const base::string16& word) {
}

void CloseDocumentWithTag(int tag) {
}

void RequestTextCheck(int document_tag,
                      const base::string16& text,
                      TextCheckCompleteCallback callback) {

}

}  // namespace spellcheck_platform
