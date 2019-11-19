// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/hunspell_engine.h"

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <utility>

#include "base/files/memory_mapped_file.h"
#include "base/time/time.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "third_party/hunspell/src/hunspell/hunspell.hxx"

using content::RenderThread;

namespace {
  // Maximum length of words we actually check.
  // 64 is the observed limits for OSX system checker.
  const size_t kMaxCheckedLen = 64;

  // Maximum length of words we provide suggestions for.
  // 24 is the observed limits for OSX system checker.
  const size_t kMaxSuggestLen = 24;

  static_assert(kMaxCheckedLen <= size_t(MAXWORDLEN),
                "MaxCheckedLen too long");
  static_assert(kMaxSuggestLen <= kMaxCheckedLen,
                "MaxSuggestLen too long");
}  // namespace

HunspellEngine::HunspellEngine(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : hunspell_enabled_(false),
      initialized_(false),
      dictionary_requested_(false),
      embedder_provider_(embedder_provider) {
  // Wait till we check the first word before doing any initializing.
}

HunspellEngine::~HunspellEngine() {
}

void HunspellEngine::Init(base::File file) {
  initialized_ = true;
  hunspell_.reset();
  bdict_file_.reset();
  file_ = std::move(file);
  hunspell_enabled_ = file_.IsValid();
  // Delay the actual initialization of hunspell until it is needed.
}

void HunspellEngine::InitializeHunspell() {
  if (hunspell_)
    return;

  bdict_file_.reset(new base::MemoryMappedFile);

  if (bdict_file_->Initialize(std::move(file_))) {
    hunspell_.reset(new Hunspell(bdict_file_->data(), bdict_file_->length()));
  } else {
    NOTREACHED() << "Could not mmap spellchecker dictionary.";
  }
}

bool HunspellEngine::CheckSpelling(const base::string16& word_to_check,
                                   int tag) {
  // Assume all words that cannot be checked are valid. Since Chrome can't
  // offer suggestions on them, either, there's no point in flagging them to
  // the user.
  bool word_correct = true;
  std::string word_to_check_utf8(base::UTF16ToUTF8(word_to_check));

  // Limit the size of checked words.
  if (word_to_check_utf8.length() <= kMaxCheckedLen) {
    // If |hunspell_| is NULL here, an error has occurred, but it's better
    // to check rather than crash.
    if (hunspell_) {
      // |hunspell_->spell| returns 0 if the word is misspelled.
      word_correct = (hunspell_->spell(word_to_check_utf8) != 0);
    }
  }

  return word_correct;
}

void HunspellEngine::FillSuggestionList(
    const base::string16& wrong_word,
    std::vector<base::string16>* optional_suggestions) {
  std::string wrong_word_utf8(base::UTF16ToUTF8(wrong_word));
  if (wrong_word_utf8.length() > kMaxSuggestLen)
    return;

  // If |hunspell_| is NULL here, an error has occurred, but it's better
  // to check rather than crash.
  // TODO(groby): Technically, it's not. We should track down the issue.
  if (!hunspell_)
    return;

  std::vector<std::string> suggestions =
      hunspell_->suggest(wrong_word_utf8);

  // Populate the vector of WideStrings.
  for (size_t i = 0; i < suggestions.size(); ++i) {
    if (i < spellcheck::kMaxSuggestions)
      optional_suggestions->push_back(base::UTF8ToUTF16(suggestions[i]));
  }
}

bool HunspellEngine::InitializeIfNeeded() {
  if (!initialized_ && !dictionary_requested_) {
    mojo::Remote<spellcheck::mojom::SpellCheckHost> spell_check_host;
    embedder_provider_->GetInterface(
        spell_check_host.BindNewPipeAndPassReceiver());
    spell_check_host->RequestDictionary();
    dictionary_requested_ = true;
    return true;
  }

  // Don't initialize if hunspell is disabled.
  if (file_.IsValid())
    InitializeHunspell();

  return !initialized_;
}

bool HunspellEngine::IsEnabled() {
  return hunspell_enabled_;
}
