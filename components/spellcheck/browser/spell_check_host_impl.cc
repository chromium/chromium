// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spell_check_host_impl.h"

#include "build/build_config.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/browser_thread.h"

SpellCheckHostImpl::SpellCheckHostImpl() = default;
SpellCheckHostImpl::~SpellCheckHostImpl() = default;

void SpellCheckHostImpl::NotifyChecked(const std::u16string& word,
                                       bool misspelled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This API requires Chrome-only features.
  return;
}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void SpellCheckHostImpl::CallSpellingService(
    const std::u16string& text,
    CallSpellingServiceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (text.empty()) {
    mojo::ReportBadMessage("Requested spelling service with empty text");
    return;
  }

  // This API requires Chrome-only features.
  std::move(callback).Run(false, std::vector<SpellCheckResult>());
}
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER) && !BUILDFLAG(ENABLE_SPELLING_SERVICE)
void SpellCheckHostImpl::RequestTextCheck(const std::u16string& text,
                                          RequestTextCheckCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (text.empty()) {
    mojo::ReportBadMessage("Requested text check with empty text");
    return;
  }

  session_bridge_.RequestTextCheck(text, std::move(callback));
}

void SpellCheckHostImpl::CheckSpelling(const std::u16string& word,
                                       CheckSpellingCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(false);
}

void SpellCheckHostImpl::FillSuggestionList(
    const std::u16string& word,
    FillSuggestionListCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run({});
}

#if BUILDFLAG(IS_WIN)
void SpellCheckHostImpl::InitializeDictionaries(
    InitializeDictionariesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(/*dictionaries=*/{}, /*custom_words=*/{},
                          /*enable=*/false);
}
#endif  // BUILDFLAG(IS_WIN)
#endif  //  BUILDFLAG(USE_BROWSER_SPELLCHECKER) &&
        //  !BUILDFLAG(ENABLE_SPELLING_SERVICE)

#if BUILDFLAG(IS_ANDROID)
void SpellCheckHostImpl::DisconnectSessionBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  session_bridge_.DisconnectSession();
}
#endif
