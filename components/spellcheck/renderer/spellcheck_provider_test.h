// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_TEST_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/spellcheck/renderer/empty_local_interface_provider.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_text_checking_completion.h"
#include "third_party/blink/public/web/web_text_checking_result.h"

struct FakeTextCheckingResult {
  size_t completion_count_ = 0;
  size_t cancellation_count_ = 0;
  blink::WebVector<blink::WebTextCheckingResult> results_;

  explicit FakeTextCheckingResult();
  ~FakeTextCheckingResult();
};

// A fake completion object for verification.
class FakeTextCheckingCompletion : public blink::WebTextCheckingCompletion {
 public:
  explicit FakeTextCheckingCompletion(FakeTextCheckingResult*);
  ~FakeTextCheckingCompletion() override;

  void DidFinishCheckingText(
      const blink::WebVector<blink::WebTextCheckingResult>& results) override;
  void DidCancelCheckingText() override;

  raw_ptr<FakeTextCheckingResult> result_;
};

// A fake SpellCheck object which can fake the number of (enabled) spell check
// languages
class FakeSpellCheck : public SpellCheck {
 public:
  explicit FakeSpellCheck(
      service_manager::LocalInterfaceProvider* embedder_provider);

  // Test-only method to set the fake language counts
  void SetFakeLanguageCounts(size_t language_count, size_t enabled_count);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // Test-only method to initialize SpellCheck object for the given locale.
  void InitializeSpellCheckForLocale(const std::string& language,
                                     bool use_hunspell);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  // Returns the current number of spell check languages.
  size_t LanguageCount() override;

  // Returns the current number of spell check languages with enabled engines.
  size_t EnabledLanguageCount() override;

 private:
  bool use_fake_counts_ = false;
  size_t language_count_ = 0;
  size_t enabled_language_count_ = 0;
};

// Faked test target, which stores sent message for verification.
class TestingSpellCheckProvider : public SpellCheckProvider,
                                  public spellcheck::mojom::SpellCheckHost {
 public:
  explicit TestingSpellCheckProvider(service_manager::LocalInterfaceProvider*);
  // Takes ownership of |spellcheck|.
  TestingSpellCheckProvider(SpellCheck* spellcheck,
                            service_manager::LocalInterfaceProvider*);

  ~TestingSpellCheckProvider() override;

  void RequestTextChecking(
      const std::u16string& text,
      std::unique_ptr<blink::WebTextCheckingCompletion> completion);

  void SetLastResults(
      const std::u16string last_request,
      blink::WebVector<blink::WebTextCheckingResult>& last_results);
  bool SatisfyRequestFromCache(const std::u16string& text,
                               blink::WebTextCheckingCompletion* completion);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  int AddCompletionForTest(
      std::unique_ptr<FakeTextCheckingCompletion> completion,
      SpellCheckProvider::HybridSpellCheckRequestInfo request_info);

  void OnRespondTextCheck(int identifier,
                          const std::u16string& line,
                          const std::vector<SpellCheckResult>& results);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  void ResetResult();

  // Variables logging CallSpellingService() mojo calls.
  std::u16string text_;
  size_t spelling_service_call_count_ = 0;
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  using RequestTextCheckParams =
      std::pair<std::u16string, RequestTextCheckCallback>;

  // Variables logging RequestTextCheck() mojo calls.
  std::vector<RequestTextCheckParams> text_check_requests_;
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  // Returns |spellcheck|.
  FakeSpellCheck* spellcheck() {
    return static_cast<FakeSpellCheck*>(spellcheck_);
  }

  base::WeakPtr<SpellCheckProvider> GetWeakPtr();

 private:
  // spellcheck::mojom::SpellCheckHost:
  void NotifyChecked(const std::u16string& word, bool misspelled) override;

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  void CallSpellingService(const std::u16string& text,
                           CallSpellingServiceCallback callback) override;
  void OnCallSpellingService(const std::u16string& text);
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void RequestTextCheck(const std::u16string&,
                        RequestTextCheckCallback) override;
  using SpellCheckProvider::CheckSpelling;
  void CheckSpelling(const std::u16string&,
                     CheckSpellingCallback) override;
  void FillSuggestionList(const std::u16string&,
                          FillSuggestionListCallback) override;
#if BUILDFLAG(IS_WIN)
  void InitializeDictionaries(InitializeDictionariesCallback callback) override;
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(IS_ANDROID)
  void DisconnectSessionBridge() override;
#endif

  // Receiver to receive the SpellCheckHost request flow.
  mojo::Receiver<spellcheck::mojom::SpellCheckHost> receiver_{this};
};

// SpellCheckProvider test fixture.
class SpellCheckProviderTest : public testing::Test {
 public:
  SpellCheckProviderTest();
  ~SpellCheckProviderTest() override;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  spellcheck::EmptyLocalInterfaceProvider embedder_provider_;
  TestingSpellCheckProvider provider_;
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_TEST_H_
