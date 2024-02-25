// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_SPELL_CHECK_HOST_IMPL_H_
#define COMPONENTS_SPELLCHECK_BROWSER_SPELL_CHECK_HOST_IMPL_H_

#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/spellcheck_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/spellcheck/browser/spellchecker_session_bridge_android.h"
#endif

#if !BUILDFLAG(ENABLE_SPELLCHECK)
#error "Spellcheck should be enabled."
#endif

class SpellCheckerSessionBridge;

// A basic implementation of SpellCheckHost without using any Chrome-only
// feature, so that there is still basic spellcheck support when those features
// are not available (e.g., on Android WebView). The full implementation
// involving Chrome-only features is in SpellCheckHostChromeImpl.
class SpellCheckHostImpl : public spellcheck::mojom::SpellCheckHost {
 public:
  SpellCheckHostImpl();

  SpellCheckHostImpl(const SpellCheckHostImpl&) = delete;
  SpellCheckHostImpl& operator=(const SpellCheckHostImpl&) = delete;

  ~SpellCheckHostImpl() override;

 protected:
  // spellcheck::mojom::SpellCheckHost:
  void NotifyChecked(const std::u16string& word, bool misspelled) override;

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  void CallSpellingService(const std::u16string& text,
                           CallSpellingServiceCallback callback) override;
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER) && !BUILDFLAG(ENABLE_SPELLING_SERVICE)
  void RequestTextCheck(const std::u16string& text,
                        RequestTextCheckCallback callback) override;

  void CheckSpelling(const std::u16string& word,
                     CheckSpellingCallback callback) override;
  void FillSuggestionList(const std::u16string& word,
                          FillSuggestionListCallback callback) override;

#if BUILDFLAG(IS_WIN)
  void InitializeDictionaries(InitializeDictionariesCallback callback) override;
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER) &&
        // !BUILDFLAG(ENABLE_SPELLING_SERVICE)

#if BUILDFLAG(IS_ANDROID)
  // spellcheck::mojom::SpellCheckHost:
  void DisconnectSessionBridge() override;
#endif

 private:
#if BUILDFLAG(IS_ANDROID)
  // Android-specific object used to query the Android spellchecker.
  SpellCheckerSessionBridge session_bridge_;
#endif
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_SPELL_CHECK_HOST_IMPL_H_
