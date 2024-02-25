// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/language_detector.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/text_classifier.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {
namespace {

using ::chromeos::machine_learning::mojom::
    REMOVED_TextSuggestSelectionRequestPtr;
using ::chromeos::machine_learning::mojom::TextAnnotationRequestPtr;
using ::chromeos::machine_learning::mojom::TextClassifier;
using ::chromeos::machine_learning::mojom::TextLanguage;
using ::chromeos::machine_learning::mojom::TextLanguagePtr;

TextLanguagePtr DefaultLanguage() {
  return TextLanguage::New("en", /*confidence=*/1);
}

}  // namespace

class FakeTextClassifier
    : public chromeos::machine_learning::mojom::TextClassifier {
 public:
  FakeTextClassifier() = default;
  ~FakeTextClassifier() override = default;

  // chromeos::machine_learning::mojom::TextClassifier:
  void Annotate(TextAnnotationRequestPtr request,
                AnnotateCallback callback) override {}

  void FindLanguages(const std::string& text,
                     FindLanguagesCallback callback) override {
    std::vector<TextLanguagePtr> languages;

    const auto it = detection_results_.find(text);
    if (it == detection_results_.end()) {
      languages.push_back(DefaultLanguage());
      std::move(callback).Run(std::move(languages));
      return;
    }

    languages.push_back(it->second.Clone());
    std::move(callback).Run(std::move(languages));
  }

  void RegisterDetectionResult(std::string text, TextLanguagePtr language) {
    detection_results_[text] = std::move(language);
  }

  void REMOVED_1(REMOVED_TextSuggestSelectionRequestPtr request,
                 REMOVED_1Callback callback) override {}

 private:
  std::map<std::string, TextLanguagePtr> detection_results_;
};

class LanguageDetectorTest : public testing::Test {
 public:
  LanguageDetectorTest() : language_detector_(&text_classifier_) {}

  LanguageDetectorTest(const LanguageDetectorTest&) = delete;
  LanguageDetectorTest& operator=(const LanguageDetectorTest&) = delete;

  const std::optional<std::string>& DetectLanguage(
      const std::string& surrounding_text,
      const std::string& selected_text) {
    base::test::TestFuture<std::optional<std::string>> future;

    language_detector_.DetectLanguage(surrounding_text, selected_text,
                                      future.GetCallback());

    detected_locale_ = future.Take();

    return detected_locale_;
  }

  FakeTextClassifier* text_classifier() { return &text_classifier_; }

 private:
  base::test::TaskEnvironment task_environment_;

  std::optional<std::string> detected_locale_;

  FakeTextClassifier text_classifier_;
  LanguageDetector language_detector_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(LanguageDetectorTest, DetectLanguage) {
  std::string selected_text = "entrada de prueba";
  std::string surrounding_text = "otros textos entrada de prueba otros textos";

  text_classifier()->RegisterDetectionResult(selected_text,
                                             TextLanguage::New("es", 0.95));
  text_classifier()->RegisterDetectionResult(surrounding_text,
                                             TextLanguage::New("es", 1));

  auto detected_locale = DetectLanguage(surrounding_text, selected_text);

  EXPECT_EQ(detected_locale, "es");
}

TEST_F(LanguageDetectorTest, DetectLanguageShouldPreferSelectedTexts) {
  std::string selected_text = "entrada de prueba";
  std::string surrounding_text = "周围的文字 entrada de prueba 周围的文字";

  text_classifier()->RegisterDetectionResult(selected_text,
                                             TextLanguage::New("es", 0.95));
  text_classifier()->RegisterDetectionResult(surrounding_text,
                                             TextLanguage::New("zh", 0.98));

  auto detected_locale = DetectLanguage(surrounding_text, selected_text);

  // Should prefer selected texts for the detected locale.
  EXPECT_EQ(detected_locale, "es");
}

TEST_F(LanguageDetectorTest, DetectLanguageFallbackToSurroundingTexts) {
  std::string selected_text = "immagini";
  std::string surrounding_text =
      "Cerca informazioni in tutto il mondo come pagine web, immagini, video, "
      "ecc.";

  text_classifier()->RegisterDetectionResult(selected_text,
                                             TextLanguage::New("la", 0.6));
  text_classifier()->RegisterDetectionResult(surrounding_text,
                                             TextLanguage::New("it", 0.98));

  auto detected_locale = DetectLanguage(surrounding_text, selected_text);

  // Should fallback to surrounding texts result since the confidence level for
  // the detected result of selected texts is too low.
  EXPECT_EQ(detected_locale, "it");
}

TEST_F(LanguageDetectorTest, DetectLanguageLowConfidence) {
  std::string selected_text = "video";
  std::string surrounding_text = "web, immagini, video, ecc";

  text_classifier()->RegisterDetectionResult(selected_text,
                                             TextLanguage::New("es", 0.4));
  text_classifier()->RegisterDetectionResult(surrounding_text,
                                             TextLanguage::New("it", 0.4));

  auto detected_locale = DetectLanguage(surrounding_text, selected_text);

  // Should return empty result since the confidence level is too low.
  EXPECT_FALSE(detected_locale.has_value());
}

}  // namespace quick_answers
