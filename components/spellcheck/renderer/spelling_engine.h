// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLING_ENGINE_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLING_ENGINE_H_

#include <string>
#include <vector>

#include "base/files/file.h"

namespace service_manager {
class LocalInterfaceProvider;
}

namespace spellcheck::mojom {
class SpellCheckHost;
}

// Creates the platform's "native" spelling engine.
class SpellingEngine* CreateNativeSpellingEngine(
    service_manager::LocalInterfaceProvider* embedder_provider);

// Interface to different spelling engines.
class SpellingEngine {
 public:
  virtual ~SpellingEngine() {}

  // Initialize spelling engine with browser-side info. Must be called before
  // any other functions are called.
  virtual void Init(base::File bdict_file) = 0;
  virtual bool InitializeIfNeeded() = 0;
  virtual bool IsEnabled() = 0;

  // Synchronously check spelling. `host` is only valid for the duration of the
  // method call and should not be stored.
  virtual bool CheckSpelling(const std::u16string& word_to_check,
                             spellcheck::mojom::SpellCheckHost& host) = 0;

  // Synchronously fill suggestion list. `host` is only valid for the duration
  // of the method call and should not be stored.
  virtual void FillSuggestionList(
      const std::u16string& wrong_word,
      spellcheck::mojom::SpellCheckHost& host,
      std::vector<std::u16string>* optional_suggestions) = 0;
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLING_ENGINE_H_

