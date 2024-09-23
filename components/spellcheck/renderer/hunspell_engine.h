// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_HUNSPELL_ENGINE_H_
#define COMPONENTS_SPELLCHECK_RENDERER_HUNSPELL_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/renderer/spelling_engine.h"

class Hunspell;

namespace base {
class MemoryMappedFile;
}

class HunspellEngine : public SpellingEngine {
 public:
  explicit HunspellEngine(
      service_manager::LocalInterfaceProvider* embedder_provider);
  ~HunspellEngine() override;

  void Init(base::File file) override;

  bool InitializeIfNeeded() override;
  bool IsEnabled() override;
  bool CheckSpelling(const std::u16string& word_to_check,
                     spellcheck::mojom::SpellCheckHost& host) override;
  void FillSuggestionList(
      const std::u16string& wrong_word,
      spellcheck::mojom::SpellCheckHost& host,
      std::vector<std::u16string>* optional_suggestions) override;

 private:
  // Initializes the Hunspell dictionary, or does nothing if |hunspell_| is
  // non-null. This blocks.
  void InitializeHunspell();

  // We memory-map the BDict file.
  std::unique_ptr<base::MemoryMappedFile> bdict_file_;

  // The hunspell dictionary in use.
  std::unique_ptr<Hunspell> hunspell_;

  base::File file_;

  // This flag is true if hunspell is enabled.
  bool hunspell_enabled_;

  // This flag is true if we have been initialized.
  // The value indicates whether we should request a
  // dictionary from the browser when the render view asks us to check the
  // spelling of a word.
  bool initialized_;

  // This flag is true if we have requested dictionary.
  bool dictionary_requested_;

  raw_ptr<service_manager::LocalInterfaceProvider> embedder_provider_;
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_HUNSPELL_ENGINE_H_
