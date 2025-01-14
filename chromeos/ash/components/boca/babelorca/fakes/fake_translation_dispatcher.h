// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TRANSLATION_DISPATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TRANSLATION_DISPATCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_translation_dispatcher.h"
#include "components/live_caption/translation_util.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

class FakeBabelOrcaTranslationDispatcher
    : public BabelOrcaTranslationDipsatcher {
 public:
  using BoundTranslateEvent = base::OnceCallback<void()>;
  using DispatchHandler =
      base::RepeatingCallback<void(BoundTranslateEvent event)>;

  FakeBabelOrcaTranslationDispatcher();
  ~FakeBabelOrcaTranslationDispatcher() override;

  // By default this class invokes `callback` with result. Using
  // InjectTranslationResult will change the value passed back to
  // the callback.  The callback can also be bound with either of these
  // values and passed to a handling function using SetDispatchHandler
  // below.
  void GetTranslation(const std::string& result,
                      const std::string& source_language,
                      const std::string& target_language,
                      captions::TranslateEventCallback callback) override;

  // Allows the caller to intercept the TranslateEventCallback which is bound
  // with the result string as specified by the injected translation result,
  // see InjectTranslationResult below, or the result value passed into
  // GetTranslation if no injected value is specified. This allows the
  // caller to delay the invocation of the translate event callback to test
  // the caption translator's ordering logic.
  void SetDispatchHandler(DispatchHandler handler) {
    dispatch_handler_ = std::move(handler);
  }

  // Unsetting the dispatch handler returns the fake back to its original
  // behavior of immediately invoking the tranlsate event callback.
  void UnsetDispatchHandler() { dispatch_handler_.Reset(); }

  void InjectTranslationResult(const std::string& translation) {
    injected_result_ = translation;
  }
  int GetNumGetTranslationCalls() { return num_translation_calls_; }
  base::WeakPtr<FakeBabelOrcaTranslationDispatcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::atomic<size_t> num_translation_calls_ = 0;
  DispatchHandler dispatch_handler_;
  std::optional<std::string> injected_result_;

  base::WeakPtrFactory<FakeBabelOrcaTranslationDispatcher> weak_ptr_factory_{
      this};
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TRANSLATION_DISPATCHER_H_
