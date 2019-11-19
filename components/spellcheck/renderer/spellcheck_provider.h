// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/web/web_text_check_client.h"

class SpellCheck;
struct SpellCheckResult;

namespace blink {
class WebTextCheckingCompletion;
struct WebTextCheckingResult;
}

namespace service_manager {
class LocalInterfaceProvider;
}

// This class deals with asynchronously invoking text spelling and grammar
// checking services provided by the browser process (host).
class SpellCheckProvider
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<SpellCheckProvider>,
      public blink::WebTextCheckClient {
 public:
  using WebTextCheckCompletions =
      base::IDMap<std::unique_ptr<blink::WebTextCheckingCompletion>>;

  SpellCheckProvider(
      content::RenderFrame* render_frame,
      SpellCheck* spellcheck,
      service_manager::LocalInterfaceProvider* embedder_provider);
  ~SpellCheckProvider() override;

  // Requests async spell and grammar checks from the platform text checker
  // available in the browser process. The function does not have special
  // handling for partial words, as Blink guarantees that no request is made
  // when typing in the middle of a word.
  void RequestTextChecking(
      const base::string16& text,
      std::unique_ptr<blink::WebTextCheckingCompletion> completion);

  // The number of ongoing spell check host requests.
  size_t pending_text_request_size() const {
    return text_check_completions_.size();
  }

  // Replace shared spellcheck data.
  void set_spellcheck(SpellCheck* spellcheck) { spellcheck_ = spellcheck; }

  // content::RenderFrameObserver:
  void FocusedElementChanged(const blink::WebElement& element) override;

 private:
  friend class TestingSpellCheckProvider;
  class DictionaryUpdateObserverImpl;

  // Sets the SpellCheckHost (for unit tests).
  void SetSpellCheckHostForTesting(
      mojo::PendingRemote<spellcheck::mojom::SpellCheckHost> host) {
    spell_check_host_.Bind(std::move(host));
  }

  // Reset dictionary_update_observer_ in TestingSpellCheckProvider dtor.
  void ResetDictionaryUpdateObserverForTesting();

  // Returns the SpellCheckHost.
  spellcheck::mojom::SpellCheckHost& GetSpellCheckHost();

  // Tries to satisfy a spellcheck request from the cache in |last_request_|.
  // Returns true (and cancels/finishes the completion) if it can, false
  // if the provider should forward the query on.
  bool SatisfyRequestFromCache(const base::string16& text,
                               blink::WebTextCheckingCompletion* completion);

  // content::RenderFrameObserver:
  void OnDestruct() override;

  // blink::WebTextCheckClient:
  bool IsSpellCheckingEnabled() const override;
  void CheckSpelling(
      const blink::WebString& text,
      size_t& offset,
      size_t& length,
      blink::WebVector<blink::WebString>* optional_suggestions) override;
  void RequestCheckingOfText(
      const blink::WebString& text,
      std::unique_ptr<blink::WebTextCheckingCompletion> completion) override;

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  void OnRespondSpellingService(int identifier,
                                const base::string16& text,
                                bool success,
                                const std::vector<SpellCheckResult>& results);
#endif

  // Returns whether |text| has word characters, i.e. whether a spellchecker
  // needs to check this text.
  bool HasWordCharacters(const base::string16& text, size_t index) const;

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void OnRespondTextCheck(
      int identifier,
      const base::string16& line,
      const std::vector<SpellCheckResult>& results);
#endif

  // Holds ongoing spellchecking operations.
  WebTextCheckCompletions text_check_completions_;

  // The last text sent to the browser process for spellchecking, and its
  // spellcheck results and WebTextCheckCompletions identifier.
  base::string16 last_request_;
  blink::WebVector<blink::WebTextCheckingResult> last_results_;
  int last_identifier_;

  // Weak pointer to shared (per renderer) spellcheck data.
  SpellCheck* spellcheck_;

  // Not owned. |embedder_provider_| should outlive SpellCheckProvider.
  service_manager::LocalInterfaceProvider* embedder_provider_;

  // Interface to the SpellCheckHost.
  mojo::Remote<spellcheck::mojom::SpellCheckHost> spell_check_host_;

  // Dictionary updated observer.
  std::unique_ptr<DictionaryUpdateObserverImpl> dictionary_update_observer_;

  base::WeakPtrFactory<SpellCheckProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SpellCheckProvider);
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_PROVIDER_H_
