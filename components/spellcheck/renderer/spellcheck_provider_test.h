// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_TEST_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_TEST_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/spellcheck/renderer/empty_local_interface_provider.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_text_checking_completion.h"
#include "third_party/blink/public/web/web_text_checking_result.h"

namespace base {
class MessageLoop;
}

struct FakeTextCheckingResult {
  size_t completion_count_ = 0;
  size_t cancellation_count_ = 0;
};

// A fake completion object for verification.
class FakeTextCheckingCompletion : public blink::WebTextCheckingCompletion {
 public:
  explicit FakeTextCheckingCompletion(FakeTextCheckingResult*);
  ~FakeTextCheckingCompletion() override;

  void DidFinishCheckingText(
      const blink::WebVector<blink::WebTextCheckingResult>& results) override;
  void DidCancelCheckingText() override;

  FakeTextCheckingResult* result_;
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
      const base::string16& text,
      std::unique_ptr<blink::WebTextCheckingCompletion> completion);

  void SetLastResults(
      const base::string16 last_request,
      blink::WebVector<blink::WebTextCheckingResult>& last_results);
  bool SatisfyRequestFromCache(const base::string16& text,
                               blink::WebTextCheckingCompletion* completion);

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  void ResetResult();

  // Variables logging CallSpellingService() mojo calls.
  base::string16 text_;
  size_t spelling_service_call_count_ = 0;
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // Variables logging RequestTextCheck() mojo calls.
  using RequestTextCheckParams =
      std::pair<base::string16, RequestTextCheckCallback>;
  std::vector<RequestTextCheckParams> text_check_requests_;
#endif

  // Returns |spellcheck|.
  SpellCheck* spellcheck() { return spellcheck_; }

 private:
  // spellcheck::mojom::SpellCheckHost:
  void RequestDictionary() override;
  void NotifyChecked(const base::string16& word, bool misspelled) override;

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  void CallSpellingService(const base::string16& text,
                           CallSpellingServiceCallback callback) override;
  void OnCallSpellingService(const base::string16& text);
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void RequestTextCheck(const base::string16&,
                        int,
                        RequestTextCheckCallback) override;
  using SpellCheckProvider::CheckSpelling;
  void CheckSpelling(const base::string16&,
                     int,
                     CheckSpellingCallback) override;
  void FillSuggestionList(const base::string16&,
                          FillSuggestionListCallback) override;
#endif

#if defined(OS_ANDROID)
  void DisconnectSessionBridge() override;
#endif

  // Message loop (if needed) to deliver the SpellCheckHost request flow.
  std::unique_ptr<base::MessageLoop> loop_;

  // Receiver to receive the SpellCheckHost request flow.
  mojo::Receiver<spellcheck::mojom::SpellCheckHost> receiver_{this};
};

// SpellCheckProvider test fixture.
class SpellCheckProviderTest : public testing::Test {
 public:
  SpellCheckProviderTest();
  ~SpellCheckProviderTest() override;

 protected:
  spellcheck::EmptyLocalInterfaceProvider embedder_provider_;
  TestingSpellCheckProvider provider_;
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_TEST_H_
