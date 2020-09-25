// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UNDERSTANDING_INTENT_GENERATOR_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UNDERSTANDING_INTENT_GENERATOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/text_classifier.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace quick_answers {

struct QuickAnswersRequest;
struct IntentInfo;
enum class IntentType;

// Generate intent from the |QuickAnswersRequest|.
class IntentGenerator {
 public:
  // Callback used when intent generation is complete.
  using IntentGeneratorCallback =
      base::OnceCallback<void(const IntentInfo& intent_info)>;

  explicit IntentGenerator(IntentGeneratorCallback complete_callback);

  IntentGenerator(const IntentGenerator&) = delete;
  IntentGenerator& operator=(const IntentGenerator&) = delete;

  virtual ~IntentGenerator();

  // Generate intent from the |request|. Virtual for testing.
  virtual void GenerateIntent(const QuickAnswersRequest& request);

 private:
  FRIEND_TEST_ALL_PREFIXES(IntentGeneratorTest,
                           TextAnnotationIntentNoAnnotation);
  FRIEND_TEST_ALL_PREFIXES(IntentGeneratorTest, TextAnnotationIntentNoEntity);
  FRIEND_TEST_ALL_PREFIXES(IntentGeneratorTest,
                           TextAnnotationIntentUnSupportedEntity);

  void LoadModelCallback(
      const QuickAnswersRequest& request,
      chromeos::machine_learning::mojom::LoadModelResult result);
  void AnnotationCallback(
      const QuickAnswersRequest& request,
      std::vector<machine_learning::mojom::TextAnnotationPtr> annotations);
  void FindLanguagesCallback(
      const QuickAnswersRequest& request,
      std::vector<machine_learning::mojom::TextLanguagePtr> languages);

  void MaybeGenerateTranslationIntent(const QuickAnswersRequest& request);

  IntentGeneratorCallback complete_callback_;
  mojo::Remote<::chromeos::machine_learning::mojom::TextClassifier>
      text_classifier_;

  base::WeakPtrFactory<IntentGenerator> weak_factory_{this};
};

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UNDERSTANDING_INTENT_GENERATOR_H_
