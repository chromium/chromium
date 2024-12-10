// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TRANSLATION_DISPATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TRANSLATION_DISPATCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "chromeos/ash/components/boca/babelorca/babel_orca_translation_dispatcher.h"
#include "components/live_caption/translation_dispatcher.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

class FakeBabelOrcaTranslationDispatcher
    : public BabelOrcaTranslationDipsatcher {
 public:
  using CustomGetTranslationHandle = base::RepeatingCallback<void(
      const std::string& result,
      const std::string& source_language,
      const std::string& target_language,
      captions::OnTranslateEventCallback callback)>;
  FakeBabelOrcaTranslationDispatcher();
  ~FakeBabelOrcaTranslationDispatcher() override;

  void GetTranslation(const std::string& result,
                      const std::string& source_language,
                      const std::string& target_language,
                      captions::OnTranslateEventCallback callback) override;

  int GetNumGetTranslationCalls() { return num_translation_calls_; }

  void InjectTranslationResult(const std::string& translation);

  base::WeakPtr<FakeBabelOrcaTranslationDispatcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::atomic<size_t> num_translation_calls_ = 0;
  std::optional<std::string> injected_result_;

  base::WeakPtrFactory<FakeBabelOrcaTranslationDispatcher> weak_ptr_factory_{
      this};
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_TRANSLATION_DISPATCHER_H_
