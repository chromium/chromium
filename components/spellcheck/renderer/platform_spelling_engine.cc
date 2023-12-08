// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/platform_spelling_engine.h"

#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"

using content::RenderThread;

PlatformSpellingEngine::PlatformSpellingEngine() = default;

PlatformSpellingEngine::~PlatformSpellingEngine() = default;


void PlatformSpellingEngine::Init(base::File bdict_file) {
}

bool PlatformSpellingEngine::InitializeIfNeeded() {
  return false;
}

bool PlatformSpellingEngine::IsEnabled() {
  return true;
}

// Synchronously query against the platform's spellchecker.
// TODO(groby): We might want async support here, too. Ideally,
// all engines share a similar path for async requests.
bool PlatformSpellingEngine::CheckSpelling(
    const std::u16string& word_to_check,
    spellcheck::mojom::SpellCheckHost& host) {
  bool word_correct = false;
  host.CheckSpelling(word_to_check, &word_correct);
  return word_correct;
}

// Synchronously query against the platform's spellchecker.
// TODO(groby): We might want async support here, too. Ideally,
// all engines share a similar path for async requests.
void PlatformSpellingEngine::FillSuggestionList(
    const std::u16string& wrong_word,
    spellcheck::mojom::SpellCheckHost& host,
    std::vector<std::u16string>* optional_suggestions) {
  host.FillSuggestionList(wrong_word, optional_suggestions);
}
