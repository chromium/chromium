// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_PLATFORM_SPELLING_ENGINE_H_
#define COMPONENTS_SPELLCHECK_RENDERER_PLATFORM_SPELLING_ENGINE_H_

#include "base/compiler_specific.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/renderer/spelling_engine.h"
#include "mojo/public/cpp/bindings/remote.h"

class PlatformSpellingEngine : public SpellingEngine {
 public:
  explicit PlatformSpellingEngine(
      service_manager::LocalInterfaceProvider* embedder_provider);
  ~PlatformSpellingEngine() override;

  void Init(base::File bdict_file) override;
  bool InitializeIfNeeded() override;
  bool IsEnabled() override;
  bool CheckSpelling(const std::u16string& word_to_check, int tag) override;
  void FillSuggestionList(
      const std::u16string& wrong_word,
      std::vector<std::u16string>* optional_suggestions) override;

 private:
  spellcheck::mojom::SpellCheckHost& GetOrBindSpellCheckHost();

  // Not owned. |embedder_provider_| outlives PlatformSpellingEngine.
  service_manager::LocalInterfaceProvider* embedder_provider_;

  mojo::Remote<spellcheck::mojom::SpellCheckHost> spell_check_host_;
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_PLATFORM_SPELLING_ENGINE_H_
