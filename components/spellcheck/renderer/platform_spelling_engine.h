// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_PLATFORM_SPELLING_ENGINE_H_
#define COMPONENTS_SPELLCHECK_RENDERER_PLATFORM_SPELLING_ENGINE_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/renderer/spelling_engine.h"
#include "mojo/public/cpp/bindings/remote.h"

class PlatformSpellingEngine : public SpellingEngine {
 public:
  PlatformSpellingEngine();
  ~PlatformSpellingEngine() override;

  void Init(base::File bdict_file) override;
  bool InitializeIfNeeded() override;
  bool IsEnabled() override;
  bool CheckSpelling(const std::u16string& word_to_check,
                     spellcheck::mojom::SpellCheckHost& host) override;
  void FillSuggestionList(
      const std::u16string& wrong_word,
      spellcheck::mojom::SpellCheckHost& host,
      std::vector<std::u16string>* optional_suggestions) override;
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_PLATFORM_SPELLING_ENGINE_H_
