// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UNDERSTANDING_INTENT_GENERATOR_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UNDERSTANDING_INTENT_GENERATOR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/quick_answers/utils/language_detector.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/text_classifier.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace quick_answers {

class SpellChecker;
struct QuickAnswersRequest;
struct IntentInfo;
enum class IntentType;

// Generate intent from the |QuickAnswersRequest|.
class IntentGenerator {
 public:
  // Callback used when intent generation is complete.
  using IntentGeneratorCallback =
      base::OnceCallback<void(const IntentInfo& intent_info)>;

  IntentGenerator(base::WeakPtr<SpellChecker> spell_checker,
                  IntentGeneratorCallback complete_callback);

  IntentGenerator(const IntentGenerator&) = delete;
  IntentGenerator& operator=(const IntentGenerator&) = delete;

  virtual ~IntentGenerator();

  // Generate intent from the |request|. Virtual for testing.
  virtual void GenerateIntent(const QuickAnswersRequest& request);

  // Flush all relevant Mojo pipes.
  void FlushForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(IntentGeneratorTest,
                           TextAnnotationIntentNoAnnotation);
  FRIEND_TEST_ALL_PREFIXES(IntentGeneratorTest, TextAnnotationIntentNoEntity);
  FRIEND_TEST_ALL_PREFIXES(IntentGeneratorTest,
                           TextAnnotationIntentUnSupportedEntity);

  void MaybeLoadTextClassifier(const QuickAnswersRequest& request);
  void CheckSpellingCallback(const QuickAnswersRequest& request,
                             bool correctness,
                             const std::string& language);

  void LoadModelCallback(
      const QuickAnswersRequest& request,
      chromeos::machine_learning::mojom::LoadModelResult result);
  void AnnotationCallback(
      const QuickAnswersRequest& request,
      std::vector<chromeos::machine_learning::mojom::TextAnnotationPtr>
          annotations);

  void MaybeGenerateTranslationIntent(const QuickAnswersRequest& request);
  void LanguageDetectorCallback(const QuickAnswersRequest& request,
                                std::optional<std::string> detected_language);

  // Owned by QuickAnswersClient;
  base::WeakPtr<SpellChecker> spell_checker_;
  IntentGeneratorCallback complete_callback_;
  mojo::Remote<::chromeos::machine_learning::mojom::TextClassifier>
      text_classifier_;
  std::unique_ptr<LanguageDetector> language_detector_;

  base::WeakPtrFactory<IntentGenerator> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UNDERSTANDING_INTENT_GENERATOR_H_
