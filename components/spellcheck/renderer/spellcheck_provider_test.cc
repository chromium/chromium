// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_provider_test.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/spellcheck_buildflags.h"

FakeTextCheckingCompletion::FakeTextCheckingCompletion(
    FakeTextCheckingResult* result)
    : result_(result) {}

FakeTextCheckingCompletion::~FakeTextCheckingCompletion() {}

void FakeTextCheckingCompletion::DidFinishCheckingText(
    const blink::WebVector<blink::WebTextCheckingResult>& results) {
  ++result_->completion_count_;
}

void FakeTextCheckingCompletion::DidCancelCheckingText() {
  ++result_->completion_count_;
  ++result_->cancellation_count_;
}

TestingSpellCheckProvider::TestingSpellCheckProvider(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : SpellCheckProvider(nullptr,
                         new SpellCheck(embedder_provider),
                         embedder_provider) {}

TestingSpellCheckProvider::TestingSpellCheckProvider(
    SpellCheck* spellcheck,
    service_manager::LocalInterfaceProvider* embedder_provider)
    : SpellCheckProvider(nullptr, spellcheck, embedder_provider) {}

TestingSpellCheckProvider::~TestingSpellCheckProvider() {
  receiver_.reset();
  // dictionary_update_observer_ must be released before deleting spellcheck_.
  ResetDictionaryUpdateObserverForTesting();
  delete spellcheck_;
}

void TestingSpellCheckProvider::RequestTextChecking(
    const base::string16& text,
    std::unique_ptr<blink::WebTextCheckingCompletion> completion) {
  if (!loop_ && !base::MessageLoopCurrent::Get())
    loop_ = std::make_unique<base::MessageLoop>();
  if (!receiver_.is_bound())
    SetSpellCheckHostForTesting(receiver_.BindNewPipeAndPassRemote());
  SpellCheckProvider::RequestTextChecking(text, std::move(completion));
  base::RunLoop().RunUntilIdle();
}

void TestingSpellCheckProvider::RequestDictionary() {}

void TestingSpellCheckProvider::NotifyChecked(const base::string16& word,
                                              bool misspelled) {}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void TestingSpellCheckProvider::CallSpellingService(
    const base::string16& text,
    CallSpellingServiceCallback callback) {
  OnCallSpellingService(text);
  std::move(callback).Run(true, std::vector<SpellCheckResult>());
}

void TestingSpellCheckProvider::OnCallSpellingService(
    const base::string16& text) {
  ++spelling_service_call_count_;
  if (!text_check_completions_.Lookup(last_identifier_)) {
    ResetResult();
    return;
  }
  text_.assign(text);
  std::unique_ptr<blink::WebTextCheckingCompletion> completion(
      text_check_completions_.Replace(last_identifier_, nullptr));
  text_check_completions_.Remove(last_identifier_);
  std::vector<blink::WebTextCheckingResult> results;
  results.push_back(
      blink::WebTextCheckingResult(blink::kWebTextDecorationTypeSpelling, 0, 5,
                                   std::vector<blink::WebString>({"hello"})));
  completion->DidFinishCheckingText(results);
  last_request_ = text;
  last_results_ = results;
}

void TestingSpellCheckProvider::ResetResult() {
  text_.clear();
}
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void TestingSpellCheckProvider::RequestTextCheck(
    const base::string16& text,
    int,
    RequestTextCheckCallback callback) {
  text_check_requests_.push_back(std::make_pair(text, std::move(callback)));
}

void TestingSpellCheckProvider::CheckSpelling(const base::string16&,
                                              int,
                                              CheckSpellingCallback) {
  NOTREACHED();
}

void TestingSpellCheckProvider::FillSuggestionList(const base::string16&,
                                                   FillSuggestionListCallback) {
  NOTREACHED();
}
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if defined(OS_ANDROID)
void TestingSpellCheckProvider::DisconnectSessionBridge() {
  NOTREACHED();
}
#endif

void TestingSpellCheckProvider::SetLastResults(
    const base::string16 last_request,
    blink::WebVector<blink::WebTextCheckingResult>& last_results) {
  last_request_ = last_request;
  last_results_ = last_results;
}

bool TestingSpellCheckProvider::SatisfyRequestFromCache(
    const base::string16& text,
    blink::WebTextCheckingCompletion* completion) {
  return SpellCheckProvider::SatisfyRequestFromCache(text, completion);
}

SpellCheckProviderTest::SpellCheckProviderTest()
    : provider_(&embedder_provider_) {}
SpellCheckProviderTest::~SpellCheckProviderTest() {}
