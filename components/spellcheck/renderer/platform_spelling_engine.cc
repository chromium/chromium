// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/platform_spelling_engine.h"

#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"

using content::RenderThread;

PlatformSpellingEngine::PlatformSpellingEngine(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : embedder_provider_(embedder_provider) {}

PlatformSpellingEngine::~PlatformSpellingEngine() = default;

spellcheck::mojom::SpellCheckHost&
PlatformSpellingEngine::GetOrBindSpellCheckHost() {
  if (spell_check_host_)
    return *spell_check_host_;

  embedder_provider_->GetInterface(
      spell_check_host_.BindNewPipeAndPassReceiver());
  return *spell_check_host_;
}

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
bool PlatformSpellingEngine::CheckSpelling(const base::string16& word_to_check,
                                           int tag) {
  bool word_correct = false;
  GetOrBindSpellCheckHost().CheckSpelling(word_to_check, tag, &word_correct);
  return word_correct;
}

// Synchronously query against the platform's spellchecker.
// TODO(groby): We might want async support here, too. Ideally,
// all engines share a similar path for async requests.
void PlatformSpellingEngine::FillSuggestionList(
    const base::string16& wrong_word,
    std::vector<base::string16>* optional_suggestions) {
  GetOrBindSpellCheckHost().FillSuggestionList(wrong_word,
                                               optional_suggestions);
}
