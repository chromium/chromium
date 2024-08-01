// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/spellcheck/renderer/spellcheck_provider.h"

#include <unordered_map>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_language.h"
#include "components/spellcheck/renderer/spellcheck_renderer_metrics.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_text_checking_completion.h"
#include "third_party/blink/public/web/web_text_checking_result.h"
#include "third_party/blink/public/web/web_text_decoration_type.h"

using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebString;
using blink::WebTextCheckingCompletion;
using blink::WebTextCheckingResult;
using blink::WebTextDecorationType;
using blink::WebVector;

static_assert(int(blink::kWebTextDecorationTypeSpelling) ==
                  int(SpellCheckResult::SPELLING),
              "mismatching enums");
static_assert(int(blink::kWebTextDecorationTypeGrammar) ==
                  int(SpellCheckResult::GRAMMAR),
              "mismatching enums");

class SpellCheckProvider::DictionaryUpdateObserverImpl
    : public DictionaryUpdateObserver {
 public:
  explicit DictionaryUpdateObserverImpl(SpellCheckProvider* owner);
  ~DictionaryUpdateObserverImpl() override;

  // DictionaryUpdateObserver:
  void OnDictionaryUpdated(const WebVector<WebString>& words_added) override;

 private:
  raw_ptr<SpellCheckProvider> owner_;
};

SpellCheckProvider::DictionaryUpdateObserverImpl::DictionaryUpdateObserverImpl(
    SpellCheckProvider* owner)
    : owner_(owner) {
  owner_->spellcheck_->AddDictionaryUpdateObserver(this);
}

SpellCheckProvider::DictionaryUpdateObserverImpl::
    ~DictionaryUpdateObserverImpl() {
  owner_->spellcheck_->RemoveDictionaryUpdateObserver(this);
}

void SpellCheckProvider::DictionaryUpdateObserverImpl::OnDictionaryUpdated(
    const WebVector<WebString>& words_added) {
  // Clear only cache. Current pending requests should continue as they are.
  owner_->last_request_.clear();
  owner_->last_results_.Assign(
      blink::WebVector<blink::WebTextCheckingResult>());

  // owner_->render_frame() is nullptr in unit tests.
  if (auto* render_frame = owner_->render_frame()) {
    DCHECK(render_frame->GetWebFrame());
    render_frame->GetWebFrame()->RemoveSpellingMarkersUnderWords(words_added);
  }
}

SpellCheckProvider::SpellCheckProvider(content::RenderFrame* render_frame,
                                       SpellCheck* spellcheck)
    : content::RenderFrameObserver(render_frame), spellcheck_(spellcheck) {
  DCHECK(spellcheck_);
  if (render_frame)  // NULL in unit tests.
    render_frame->GetWebFrame()->SetTextCheckClient(this);

  dictionary_update_observer_ =
      std::make_unique<DictionaryUpdateObserverImpl>(this);
}

SpellCheckProvider::~SpellCheckProvider() {
}

void SpellCheckProvider::ResetDictionaryUpdateObserverForTesting() {
  dictionary_update_observer_.reset();
}

void SpellCheckProvider::RequestTextChecking(
    const std::u16string& text,
    std::unique_ptr<WebTextCheckingCompletion> completion) {
  // Ignore invalid requests.
  if (text.empty() || !HasWordCharacters(text, 0)) {
    completion->DidCancelCheckingText();
    return;
  }

  // Try to satisfy check from cache.
  if (SatisfyRequestFromCache(text, completion.get()))
    return;

  // Send this text to a browser. A browser checks the user profile and send
  // this text to the Spelling service only if a user enables this feature.
  last_request_.clear();
  last_results_.Assign(blink::WebVector<blink::WebTextCheckingResult>());
  last_identifier_ = text_check_completions_.Add(std::move(completion));

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (spellcheck::UseBrowserSpellChecker()) {
#if BUILDFLAG(IS_WIN)
    if (base::FeatureList::IsEnabled(
            spellcheck::kWinDelaySpellcheckServiceInit) &&
        !dictionaries_loaded_) {
      // Initialize the spellcheck service on demand (this spellcheck request
      // could be the result of the first click in editable content), then
      // complete the text check request when the dictionaries are loaded.
      // The delayed spell check service initialization sequence, starting from
      // when the user clicks in editable content, is as follows:
      // - SpellcheckProvider::RequestTextChecking (Renderer, this method)
      // - SpellCheckHostChromeImpl::InitializeDictionaries (Browser)
      // - SpellcheckService::InitializeDictionaries (Browser)
      // - SpellCheckHostChromeImpl::OnDictionariesInitialized (Browser)
      // - SpellcheckProvider::OnRespondInitializeDictionaries (Renderer)
      GetSpellCheckHost().InitializeDictionaries(
          base::BindOnce(&SpellCheckProvider::OnRespondInitializeDictionaries,
                         weak_factory_.GetWeakPtr(), text));
      return;
    }
#endif  // BUILDFLAG(IS_WIN)

    RequestTextCheckingFromBrowser(text);
  }
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  if (!spellcheck::UseBrowserSpellChecker()) {
    GetSpellCheckHost().CallSpellingService(
        text,
        base::BindOnce(&SpellCheckProvider::OnRespondSpellingService,
                       weak_factory_.GetWeakPtr(), last_identifier_, text));
  }
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)
}

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellCheckProvider::RequestTextCheckingFromBrowser(
    const std::u16string& text) {
  DCHECK(spellcheck::UseBrowserSpellChecker());
#if BUILDFLAG(IS_WIN)

  // Determine whether a hybrid check is needed.
  bool use_hunspell = spellcheck_->EnabledLanguageCount() > 0;
  bool use_native =
      spellcheck_->EnabledLanguageCount() != spellcheck_->LanguageCount();

  if (!use_hunspell && !use_native) {
    OnRespondTextCheck(last_identifier_, text, /*results=*/{});
    return;
  }

  if (!use_native) {
    // No language can be handled by the native spell checker. Use the regular
    // Hunspell code path.
    GetSpellCheckHost().CallSpellingService(
        text,
        base::BindOnce(&SpellCheckProvider::OnRespondSpellingService,
                       weak_factory_.GetWeakPtr(), last_identifier_, text));
    return;
  }

  // Some languages can be handled by the native spell checker. Use the
  // regular browser spell check code path. If hybrid spell check is
  // required (i.e. some locales must be checked by Hunspell), misspellings
  // from the native spell checker will be double-checked with Hunspell in
  // the |OnRespondTextCheck| callback.
  hybrid_requests_info_[last_identifier_] = {/*used_hunspell=*/use_hunspell,
                                             /*used_native=*/use_native,
                                             base::TimeTicks::Now()};
#endif  // BUILDFLAG(IS_WIN)

  // Text check (unified request for grammar and spell check) is only
  // available for browser process, so we ask the system spellchecker
  // over mojo or return an empty result if the checker is not available.
  GetSpellCheckHost().RequestTextCheck(
      text, base::BindOnce(&SpellCheckProvider::OnRespondTextCheck,
                           weak_factory_.GetWeakPtr(), last_identifier_, text));
}

#if BUILDFLAG(IS_WIN)
void SpellCheckProvider::OnRespondInitializeDictionaries(
    const std::u16string& text,
    std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries,
    const std::vector<std::string>& custom_words,
    bool enable) {
  DCHECK(!dictionaries_loaded_);
  dictionaries_loaded_ = true;

  // Because the SpellChecker and SpellCheckHost mojo interfaces use different
  // channels, there is no guarantee that the SpellChecker::Initialize response
  // will be received before the SpellCheckHost::InitializeDictionaries
  // callback. If the order is reversed, no spellcheck will be performed since
  // the renderer side thinks there are no dictionaries available. Ensure that
  // the SpellChecker is initialized before performing a spellcheck.
  spellcheck_->Initialize(std::move(dictionaries), custom_words, enable);

  RequestTextCheckingFromBrowser(text);
}
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

void SpellCheckProvider::FocusedElementChanged(
    const blink::WebElement& unused) {
#if BUILDFLAG(IS_ANDROID)
  if (!spell_check_host_.is_bound())
    return;

  WebLocalFrame* frame = render_frame()->GetWebFrame();
  WebElement element = frame->GetDocument().IsNull()
                           ? WebElement()
                           : frame->GetDocument().FocusedElement();
  bool enabled = !element.IsNull() && element.IsEditable();
  if (!enabled)
    GetSpellCheckHost().DisconnectSessionBridge();
#endif  // BUILDFLAG(IS_ANDROID)
}

spellcheck::mojom::SpellCheckHost& SpellCheckProvider::GetSpellCheckHost() {
  if (spell_check_host_) {
    return *spell_check_host_.get();
  }

  // We shodulnt't get here in tests, `spell_check_host_` should have been set.
  CHECK(render_frame());

  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      spell_check_host_.BindNewPipeAndPassReceiver());
  return *spell_check_host_.get();
}

bool SpellCheckProvider::IsSpellCheckingEnabled() const {
  return spellcheck_->IsSpellcheckEnabled();
}

void SpellCheckProvider::CheckSpelling(
    const WebString& text,
    size_t& offset,
    size_t& length,
    blink::WebVector<blink::WebString>* optional_suggestions) {
  std::u16string word = text.Utf16();

  if (optional_suggestions) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    base::TimeTicks suggestions_start = base::TimeTicks::Now();
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    // Retrieve suggestions from Hunspell. Windows platform spellchecker
    // suggestions are retrieved in SpellingMenuObserver::InitMenu on the
    // browser process side to avoid a blocking IPC.
    spellcheck::PerLanguageSuggestions per_language_suggestions;
    spellcheck_->SpellCheckWord(word, GetSpellCheckHost(), &offset, &length,
                                &per_language_suggestions);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    spellcheck_renderer_metrics::RecordHunspellSuggestionDuration(
        base::TimeTicks::Now() - suggestions_start);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

    std::vector<std::u16string> suggestions;
    spellcheck::FillSuggestions(per_language_suggestions, &suggestions);
    WebVector<WebString> web_suggestions(suggestions.size());
    base::ranges::transform(suggestions, web_suggestions.begin(),
                            [](const auto& suggestion) {
                              return WebString::FromUTF16(suggestion);
                            });
    *optional_suggestions = web_suggestions;
    spellcheck_renderer_metrics::RecordCheckedTextLengthWithSuggestions(
        base::saturated_cast<int>(word.size()));
  } else {
    spellcheck_->SpellCheckWord(word, GetSpellCheckHost(), &offset, &length,
                                /* optional suggestions vector */ nullptr);
    spellcheck_renderer_metrics::RecordCheckedTextLengthNoSuggestions(
        base::saturated_cast<int>(word.size()));

    // If optional_suggestions is not requested, the API is called
    // for marking. So we use this for counting markable words.
    GetSpellCheckHost().NotifyChecked(word, 0 < length);
  }
}

void SpellCheckProvider::RequestCheckingOfText(
    const WebString& text,
    std::unique_ptr<WebTextCheckingCompletion> completion) {
  RequestTextChecking(text.Utf16(), std::move(completion));
  spellcheck_renderer_metrics::RecordAsyncCheckedTextLength(
      base::saturated_cast<int>(text.length()));
}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
void SpellCheckProvider::OnRespondSpellingService(
    int identifier,
    const std::u16string& line,
    bool success,
    const std::vector<SpellCheckResult>& results) {
  if (!text_check_completions_.Lookup(identifier))
    return;
  std::unique_ptr<WebTextCheckingCompletion> completion(
      text_check_completions_.Replace(identifier, nullptr));
  text_check_completions_.Remove(identifier);

  // If |success| is false, we use local spellcheck as a fallback.
  if (!success) {
    spellcheck_->RequestTextChecking(line, std::move(completion),
                                     weak_factory_.GetWeakPtr());
    return;
  }

  // Double-check the returned spellchecking results with Hunspell to visualize
  // the differences between ours and the enhanced spell checker.
  blink::WebVector<blink::WebTextCheckingResult> textcheck_results;
  spellcheck_->CreateTextCheckingResults(
      SpellCheck::USE_HUNSPELL_FOR_GRAMMAR, GetSpellCheckHost(),
      /*line_offset=*/0, line, results, &textcheck_results);
  completion->DidFinishCheckingText(textcheck_results);

  // Cache the request and the converted results.
  last_request_ = line;
  last_results_.swap(textcheck_results);
}
#endif

bool SpellCheckProvider::HasWordCharacters(const std::u16string& text,
                                           size_t index) const {
  const char16_t* data = text.data();
  size_t length = text.length();
  while (index < length) {
    uint32_t code = 0;
    U16_NEXT(data, index, length, code);
    UErrorCode error = U_ZERO_ERROR;
    if (uscript_getScript(code, &error) != USCRIPT_COMMON)
      return true;
  }
  return false;
}

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellCheckProvider::OnRespondTextCheck(
    int identifier,
    const std::u16string& line,
    const std::vector<SpellCheckResult>& results) {
  DCHECK(spellcheck_);
  if (!text_check_completions_.Lookup(identifier))
    return;
  std::unique_ptr<WebTextCheckingCompletion> completion(
      text_check_completions_.Replace(identifier, nullptr));
  text_check_completions_.Remove(identifier);
  blink::WebVector<blink::WebTextCheckingResult> textcheck_results;

  SpellCheck::ResultFilter result_filter = SpellCheck::DO_NOT_MODIFY;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  const auto& request_info = hybrid_requests_info_.find(identifier);
  if (spellcheck::UseBrowserSpellChecker() &&
      request_info != hybrid_requests_info_.end() &&
      request_info->second.used_hunspell && request_info->second.used_native) {
    // Not all locales could be checked by the native spell checker. Verify each
    // mistake against Hunspell in the locales that weren't checked.
    result_filter = SpellCheck::USE_HUNSPELL_FOR_HYBRID_CHECK;
  }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  spellcheck_->CreateTextCheckingResults(result_filter, GetSpellCheckHost(),
                                         /*line_offset=*/0, line, results,
                                         &textcheck_results);

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (request_info != hybrid_requests_info_.end()) {
    spellcheck_renderer_metrics::RecordSpellcheckDuration(
        base::TimeTicks::Now() - request_info->second.request_start_ticks,
        request_info->second.used_hunspell, request_info->second.used_native);
    hybrid_requests_info_.erase(request_info);
  }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  completion->DidFinishCheckingText(textcheck_results);

  // Cache the request and the converted results.
  last_request_ = line;
  last_results_.swap(textcheck_results);
}
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

bool SpellCheckProvider::SatisfyRequestFromCache(
    const std::u16string& text,
    WebTextCheckingCompletion* completion) {
  size_t last_length = last_request_.length();
  if (!last_length)
    return false;

  // Send back the |last_results_| if the |last_request_| is a substring of
  // |text| and |text| does not have more words to check. Provider cannot cancel
  // the spellcheck request here, because WebKit might have discarded the
  // previous spellcheck results and erased the spelling markers in response to
  // the user editing the text.
  std::u16string request(text);
  size_t text_length = request.length();
  if (text_length >= last_length &&
      !request.compare(0, last_length, last_request_)) {
    if (text_length == last_length || !HasWordCharacters(text, last_length)) {
      completion->DidFinishCheckingText(last_results_);
      return true;
    }
  }

  // Create a subset of the cached results and return it if the given text is a
  // substring of the cached text.
  if (text_length < last_length &&
      !last_request_.compare(0, text_length, request)) {
    size_t result_size = 0;
    for (size_t i = 0; i < last_results_.size(); ++i) {
      size_t start = last_results_[i].location;
      size_t end = start + last_results_[i].length;
      if (start <= text_length && end <= text_length)
        ++result_size;
    }
    blink::WebVector<blink::WebTextCheckingResult> results(last_results_.data(),
                                                           result_size);
    completion->DidFinishCheckingText(results);
    return true;
  }

  return false;
}

void SpellCheckProvider::OnDestruct() {
  delete this;
}
