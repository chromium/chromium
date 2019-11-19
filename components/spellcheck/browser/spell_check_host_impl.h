// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_SPELL_CHECK_HOST_IMPL_H_
#define COMPONENTS_SPELLCHECK_BROWSER_SPELL_CHECK_HOST_IMPL_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

#if defined(OS_ANDROID)
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
  ~SpellCheckHostImpl() override;

  static void Create(
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver);

 protected:
  // spellcheck::mojom::SpellCheckHost:
  void RequestDictionary() override;
  void NotifyChecked(const base::string16& word, bool misspelled) override;

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  void CallSpellingService(const base::string16& text,
                           CallSpellingServiceCallback callback) override;
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void RequestTextCheck(const base::string16& text,
                        int route_id,
                        RequestTextCheckCallback callback) override;
  void CheckSpelling(const base::string16& word,
                     int route_id,
                     CheckSpellingCallback callback) override;
  void FillSuggestionList(const base::string16& word,
                          FillSuggestionListCallback callback) override;
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if defined(OS_ANDROID)
  // spellcheck::mojom::SpellCheckHost:
  void DisconnectSessionBridge() override;
#endif

 private:
#if defined(OS_ANDROID)
  // Android-specific object used to query the Android spellchecker.
  SpellCheckerSessionBridge session_bridge_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SpellCheckHostImpl);
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_SPELL_CHECK_HOST_IMPL_H_
