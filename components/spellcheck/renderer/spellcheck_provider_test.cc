// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_provider_test.h"

#include <memory>

#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/hunspell_engine.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_language.h"
#include "components/spellcheck/spellcheck_buildflags.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

namespace {
base::FilePath GetHunspellDirectory() {
  base::FilePath hunspell_directory;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                              &hunspell_directory)) {
    return base::FilePath();
  }

  hunspell_directory = hunspell_directory.AppendASCII("third_party");
  hunspell_directory = hunspell_directory.AppendASCII("hunspell_dictionaries");
  return hunspell_directory;
}
}  // namespace
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

FakeTextCheckingResult::FakeTextCheckingResult() = default;
FakeTextCheckingResult::~FakeTextCheckingResult() = default;

FakeTextCheckingCompletion::FakeTextCheckingCompletion(
    FakeTextCheckingResult* result)
    : result_(result) {}

FakeTextCheckingCompletion::~FakeTextCheckingCompletion() {}

void FakeTextCheckingCompletion::DidFinishCheckingText(
    const blink::WebVector<blink::WebTextCheckingResult>& results) {
  ++result_->completion_count_;
  result_->results_ = results;
}

void FakeTextCheckingCompletion::DidCancelCheckingText() {
  ++result_->completion_count_;
  ++result_->cancellation_count_;
}

FakeSpellCheck::FakeSpellCheck(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : SpellCheck(embedder_provider) {}

void FakeSpellCheck::SetFakeLanguageCounts(size_t language_count,
                                           size_t enabled_count) {
  use_fake_counts_ = true;
  language_count_ = language_count;
  enabled_language_count_ = enabled_count;
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void FakeSpellCheck::InitializeSpellCheckForLocale(const std::string& language,
                                                   bool use_hunspell) {
  // Non-Hunspell case is passed invalid file to SpellcheckLanguage::Init.
  base::File file;

  if (use_hunspell) {
    base::FilePath hunspell_directory = GetHunspellDirectory();
    EXPECT_FALSE(hunspell_directory.empty());
    base::FilePath hunspell_file_path =
        spellcheck::GetVersionedFileName(language, hunspell_directory);
    file.Initialize(hunspell_file_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
    EXPECT_TRUE(file.IsValid()) << hunspell_file_path << " is not valid"
                                << file.ErrorToString(file.GetLastFileError());
  }

  // Add the SpellcheckLanguage manually to the SpellCheck object.
  SpellCheck::languages_.push_back(
      std::make_unique<SpellcheckLanguage>(embedder_provider_));
  SpellCheck::languages_.back()->platform_spelling_engine_ =
      std::make_unique<HunspellEngine>(embedder_provider_);
  SpellCheck::languages_.back()->Init(std::move(file), language);
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

size_t FakeSpellCheck::LanguageCount() {
  return use_fake_counts_ ? language_count_ : SpellCheck::LanguageCount();
}

size_t FakeSpellCheck::EnabledLanguageCount() {
  return use_fake_counts_ ? enabled_language_count_
                          : SpellCheck::EnabledLanguageCount();
}

TestingSpellCheckProvider::TestingSpellCheckProvider(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : SpellCheckProvider(nullptr, new FakeSpellCheck(embedder_provider)) {
  SetSpellCheckHostForTesting(receiver_.BindNewPipeAndPassRemote());
}

TestingSpellCheckProvider::TestingSpellCheckProvider(
    SpellCheck* spellcheck,
    service_manager::LocalInterfaceProvider* embedder_provider)
    : SpellCheckProvider(nullptr, spellcheck) {
  SetSpellCheckHostForTesting(receiver_.BindNewPipeAndPassRemote());
}

TestingSpellCheckProvider::~TestingSpellCheckProvider() {
  receiver_.reset();
  // dictionary_update_observer_ must be released before deleting spellcheck_.
  ResetDictionaryUpdateObserverForTesting();
  delete spellcheck_;
}

void TestingSpellCheckProvider::RequestTextChecking(
    const std::u16string& text,
    std::unique_ptr<blink::WebTextCheckingCompletion> completion) {
  SpellCheckProvider::RequestTextChecking(text, std::move(completion));
  base::RunLoop().RunUntilIdle();
}

void TestingSpellCheckProvider::NotifyChecked(const std::u16string& word,
                                              bool misspelled) {}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void TestingSpellCheckProvider::CallSpellingService(
    const std::u16string& text,
    CallSpellingServiceCallback callback) {
  OnCallSpellingService(text);
  std::move(callback).Run(true, std::vector<SpellCheckResult>());
}

void TestingSpellCheckProvider::OnCallSpellingService(
    const std::u16string& text) {
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
    const std::u16string& text,
    RequestTextCheckCallback callback) {
  text_check_requests_.push_back(std::make_pair(text, std::move(callback)));
}

void TestingSpellCheckProvider::CheckSpelling(const std::u16string&,
                                              CheckSpellingCallback) {
  NOTREACHED_IN_MIGRATION();
}

void TestingSpellCheckProvider::FillSuggestionList(const std::u16string&,
                                                   FillSuggestionListCallback) {
  NOTREACHED_IN_MIGRATION();
}

#if BUILDFLAG(IS_WIN)
void TestingSpellCheckProvider::InitializeDictionaries(
    InitializeDictionariesCallback callback) {
  if (base::FeatureList::IsEnabled(
          spellcheck::kWinDelaySpellcheckServiceInit)) {
    std::move(callback).Run(/*dictionaries=*/{}, /*custom_words=*/{},
                            /*enable=*/false);
    return;
  }

  NOTREACHED_IN_MIGRATION();
}
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(IS_ANDROID)
void TestingSpellCheckProvider::DisconnectSessionBridge() {
  NOTREACHED_IN_MIGRATION();
}
#endif

void TestingSpellCheckProvider::SetLastResults(
    const std::u16string last_request,
    blink::WebVector<blink::WebTextCheckingResult>& last_results) {
  last_request_ = last_request;
  last_results_ = last_results;
}

bool TestingSpellCheckProvider::SatisfyRequestFromCache(
    const std::u16string& text,
    blink::WebTextCheckingCompletion* completion) {
  return SpellCheckProvider::SatisfyRequestFromCache(text, completion);
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
int TestingSpellCheckProvider::AddCompletionForTest(
    std::unique_ptr<FakeTextCheckingCompletion> completion,
    SpellCheckProvider::HybridSpellCheckRequestInfo request_info) {
  int id =
      SpellCheckProvider::text_check_completions_.Add(std::move(completion));
  SpellCheckProvider::hybrid_requests_info_[id] = request_info;
  return id;
}

void TestingSpellCheckProvider::OnRespondTextCheck(
    int identifier,
    const std::u16string& line,
    const std::vector<SpellCheckResult>& results) {
  SpellCheckProvider::OnRespondTextCheck(identifier, line, results);
  base::RunLoop().RunUntilIdle();
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

base::WeakPtr<SpellCheckProvider> TestingSpellCheckProvider::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

SpellCheckProviderTest::SpellCheckProviderTest()
    : provider_(&embedder_provider_) {}
SpellCheckProviderTest::~SpellCheckProviderTest() {}
