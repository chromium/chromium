// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/understanding/intent_generator.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/quick_answers_test_base.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/components/quick_answers/utils/spell_checker.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/text_classifier.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {
namespace {

using ::chromeos::machine_learning::FakeServiceConnectionImpl;
using ::chromeos::machine_learning::mojom::TextAnnotation;
using ::chromeos::machine_learning::mojom::TextAnnotationPtr;
using ::chromeos::machine_learning::mojom::TextEntity;
using ::chromeos::machine_learning::mojom::TextEntityData;
using ::chromeos::machine_learning::mojom::TextEntityPtr;
using ::chromeos::machine_learning::mojom::TextLanguage;
using ::chromeos::machine_learning::mojom::TextLanguagePtr;

TextLanguagePtr DefaultLanguage() {
  return TextLanguage::New("en", /* confidence */ 1);
}

class FakeSpellChecker : public SpellChecker {
 public:
  FakeSpellChecker(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : SpellChecker(url_loader_factory) {}
  ~FakeSpellChecker() override = default;

  // SpellChecker:
  void CheckSpelling(const std::string& word,
                     CheckSpellingCallback callback) override {
    std::move(callback).Run(dictionary_.find(word) != dictionary_.end());
  }

  void AddWordToDictionary(std::string word) { dictionary_.insert(word); }

 private:
  std::set<std::string> dictionary_;
};

}  // namespace

class IntentGeneratorTest : public QuickAnswersTestBase {
 public:
  IntentGeneratorTest() = default;

  IntentGeneratorTest(const IntentGeneratorTest&) = delete;
  IntentGeneratorTest& operator=(const IntentGeneratorTest&) = delete;

  void SetUp() override {
    QuickAnswersTestBase::SetUp();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    spell_checker_ =
        std::make_unique<FakeSpellChecker>(test_shared_loader_factory_);
    intent_generator_ = std::make_unique<IntentGenerator>(
        spell_checker_->GetWeakPtr(),
        base::BindOnce(&IntentGeneratorTest::IntentGeneratorTestCallback,
                       base::Unretained(this)));

    QuickAnswersState::Get()->set_use_text_annotator_for_testing();
  }

  void TearDown() override {
    intent_generator_.reset();
    spell_checker_.reset();
    QuickAnswersTestBase::TearDown();
  }

  void IntentGeneratorTestCallback(const IntentInfo& intent_info) {
    intent_info_ = intent_info;
  }

  // Flush all relevant Mojo pipes.
  void FlushForTesting() {
    intent_generator_->FlushForTesting();
    fake_service_connection_.FlushForTesting();
  }

 protected:
  void UseFakeServiceConnection(
      const std::vector<TextAnnotationPtr>& annotations =
          std::vector<TextAnnotationPtr>(),
      const std::vector<TextLanguagePtr>& languages =
          std::vector<TextLanguagePtr>()) {
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();

    fake_service_connection_.SetOutputAnnotation(annotations);
    fake_service_connection_.SetOutputLanguages(languages);
  }

  FakeSpellChecker* spell_checker() { return spell_checker_.get(); }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<FakeSpellChecker> spell_checker_;
  std::unique_ptr<IntentGenerator> intent_generator_;
  IntentInfo intent_info_;
  FakeServiceConnectionImpl fake_service_connection_;
};

TEST_F(IntentGeneratorTest, TranslationIntent) {
  std::vector<TextLanguagePtr> languages;
  languages.push_back(DefaultLanguage());
  UseFakeServiceConnection({}, languages);

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  fake_quick_answers_state()->set_application_locale("es");
  fake_quick_answers_state()->set_preferred_languages("es");
  intent_generator_->GenerateIntent(request);

  FlushForTesting();

  // Should generate translation intent.
  EXPECT_EQ(IntentType::kTranslation, intent_info_.intent_type);
  EXPECT_EQ("quick answers", intent_info_.intent_text);
  EXPECT_EQ("es", intent_info_.device_language);
  EXPECT_EQ("en", intent_info_.source_language);
}

TEST_F(IntentGeneratorTest, TranslationIntentWithSubtag) {
  std::vector<TextLanguagePtr> languages;
  languages.push_back(TextLanguage::New("en-US", /* confidence */ 1));
  UseFakeServiceConnection({}, languages);

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  fake_quick_answers_state()->set_application_locale("es");
  fake_quick_answers_state()->set_preferred_languages("es");
  intent_generator_->GenerateIntent(request);

  FlushForTesting();

  // Should generate translation intent.
  EXPECT_EQ(IntentType::kTranslation, intent_info_.intent_type);
  EXPECT_EQ("quick answers", intent_info_.intent_text);
  EXPECT_EQ("es", intent_info_.device_language);
  // Should drop substag for source language.
  EXPECT_EQ("en", intent_info_.source_language);
}

TEST_F(IntentGeneratorTest, TranslationIntentSameLanguage) {
  std::vector<TextLanguagePtr> languages;
  languages.push_back(DefaultLanguage());
  UseFakeServiceConnection({}, languages);

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  fake_quick_answers_state()->set_application_locale("en");
  fake_quick_answers_state()->set_preferred_languages("en");
  intent_generator_->GenerateIntent(request);

  FlushForTesting();

  // Should not generate translation intent since the detected language is the
  // same as system language.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("quick answers", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TranslationIntentPreferredLocale) {
  std::vector<TextLanguagePtr> languages;
  languages.push_back(DefaultLanguage());
  UseFakeServiceConnection({}, languages);

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  fake_quick_answers_state()->set_application_locale("es");
  fake_quick_answers_state()->set_preferred_languages("es,en,zh");
  intent_generator_->GenerateIntent(request);

  FlushForTesting();

  // Should not generate translation intent since the detected language is in
  // the preferred languages list.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("quick answers", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TranslationIntentPreferredLanguage) {
  std::vector<TextLanguagePtr> languages;
  languages.push_back(DefaultLanguage());
  UseFakeServiceConnection({}, languages);

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  fake_quick_answers_state()->set_application_locale("es");
  fake_quick_answers_state()->set_preferred_languages("es-MX,en-US,zh-CN");
  intent_generator_->GenerateIntent(request);

  FlushForTesting();

  // Should not generate translation intent since the detected language is in
  // the preferred languages list.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("quick answers", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TranslationIntentTextLengthAboveThreshold) {
  std::vector<TextLanguagePtr> languages;
  languages.push_back(DefaultLanguage());
  UseFakeServiceConnection({}, languages);

  QuickAnswersRequest request;
  request.selected_text =
      "Search the world's information, including webpages, images, videos and "
      "more. Google has many special features to help you find exactly what "
      "you're looking ...";
  fake_quick_answers_state()->set_application_locale("es");
  fake_quick_answers_state()->set_preferred_languages("es");
  intent_generator_->GenerateIntent(request);

  FlushForTesting();

  // Should not generate translation intent since the length of the selected
  // text is above the threshold.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ(
      "Search the world's information, including webpages, images, videos and "
      "more. Google has many special features to help you find exactly what "
      "you're looking ...",
      intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TranslationIntentWithAnnotation) {
  QuickAnswersRequest request;
  request.selected_text = "unfathomable";
  fake_quick_answers_state()->set_application_locale("es");
  fake_quick_answers_state()->set_preferred_languages("es");

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(TextEntity::New(
      "dictionary",                            // Entity name.
      1.0,                                     // Confidence score.
      TextEntityData::NewNumericValue(0.0)));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,   // Start offset.
                                                   12,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());
  std::vector<TextLanguagePtr> languages;
  languages.push_back(DefaultLanguage());
  UseFakeServiceConnection(annotations, languages);

  intent_generator_->GenerateIntent(request);

  FlushForTesting();

  // Should generate dictionary intent which is prioritized against
  // translation.
  EXPECT_EQ(IntentType::kDictionary, intent_info_.intent_type);
  EXPECT_EQ("unfathomable", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TranslationIntentDeviceLanguageNotSet) {
  std::vector<TextLanguagePtr> languages;
  languages.push_back(DefaultLanguage());
  UseFakeServiceConnection({}, languages);

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  intent_generator_->GenerateIntent(request);

  FlushForTesting();

  // Should not generate translation intent since the device language is not
  // set.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("quick answers", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationDefinitionIntent) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "unfathomable";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(TextEntity::New(
      "dictionary",                            // Entity name.
      1.0,                                     // Confidence score.
      TextEntityData::NewNumericValue(0.0)));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,   // Start offset.
                                                   12,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  FlushForTesting();

  // Should generate dictionary intent.
  EXPECT_EQ(IntentType::kDictionary, intent_info_.intent_type);
  EXPECT_EQ("unfathomable", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest,
       TextAnnotationDefinitionIntentExtraCharsBelowThreshold) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "“unfathomable”";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(TextEntity::New(
      "dictionary",                            // Entity name.
      1.0,                                     // Confidence score.
      TextEntityData::NewNumericValue(0.0)));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(1,   // Start offset.
                                                   13,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  FlushForTesting();

  // Should generate dictionary intent since the extra characters is below the
  // threshold.
  EXPECT_EQ(IntentType::kDictionary, intent_info_.intent_type);
  EXPECT_EQ("unfathomable", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest,
       TextAnnotationDefinitionIntentExtraCharsAboveThreshold) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "the unfathomable";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(TextEntity::New(
      "dictionary",                            // Entity name.
      1.0,                                     // Confidence score.
      TextEntityData::NewNumericValue(0.0)));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(4,   // Start offset.
                                                   16,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  FlushForTesting();

  // Should not generate dictionary intent since the extra characters is above
  // the threshold.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("unfathomable", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationUnitIntentExtraChars) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "23 cm to";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(TextEntity::New(
      "unit",                                  // Entity name.
      1.0,                                     // Confidence score.
      TextEntityData::NewNumericValue(0.0)));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,  // Start offset.
                                                   5,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  FlushForTesting();

  // Should generate unit conversion intent.
  EXPECT_EQ(IntentType::kUnit, intent_info_.intent_type);
  EXPECT_EQ("23 cm", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationUnitIntentUtf16Char) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "350°F";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(TextEntity::New(
      "unit",                                  // Entity name.
      1.0,                                     // Confidence score.
      TextEntityData::NewNumericValue(0.0)));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,  // Start offset.
                                                   5,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  FlushForTesting();

  // Should generate unit conversion intent.
  EXPECT_EQ(IntentType::kUnit, intent_info_.intent_type);
  EXPECT_EQ("350°F", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationUnitIntentExtraCharsAboveThreshold) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "23 cm is equal to 9.06 inches";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(TextEntity::New(
      "unit",                                  // Entity name.
      1.0,                                     // Confidence score.
      TextEntityData::NewNumericValue(0.0)));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,  // Start offset.
                                                   5,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  FlushForTesting();

  // Should not generate unit conversion intent since the extra characters is
  // above the threshold.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("23 cm", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationIntentNoAnnotation) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "the unfathomable reaches of space";

  std::vector<TextAnnotationPtr> annotations;
  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);
  FlushForTesting();

  // Should generate unknown intent since no annotation found.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("the unfathomable reaches of space", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationIntentNoEntity) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "the unfathomable reaches of space";

  std::vector<TextEntityPtr> entities;
  auto dictionary_annotation = TextAnnotation::New(4,   // Start offset.
                                                   16,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());
  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);
  FlushForTesting();

  // Should generate unknown intent since no entity found.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("the unfathomable reaches of space", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationIntentUnSupportedEntity) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "the unfathomable reaches of space";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(TextEntity::New(
      "something_else",                        // Entity name.
      1.0,                                     // Confidence score.
      TextEntityData::NewNumericValue(0.0)));  // Data extracted.

  auto some_annotation = TextAnnotation::New(4,   // Start offset.
                                             16,  // End offset.
                                             std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(some_annotation->Clone());
  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);
  FlushForTesting();

  // Should generate unknown intent unsupported entity is provided.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("the unfathomable reaches of space", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, ShouldTriggerForSingleWordInDictionary) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::features::kQuickAnswersAlwaysTriggerForSingleWord);

  const std::string kWord = "single";

  // No Annotation provided.
  std::vector<TextAnnotationPtr> annotations;
  UseFakeServiceConnection(annotations);

  // Add word to the dictionary.
  spell_checker()->AddWordToDictionary(kWord);

  // Word selected.
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = kWord;

  intent_generator_->GenerateIntent(*quick_answers_request);
  task_environment_.RunUntilIdle();

  // Should generate dictionary intent.
  EXPECT_EQ(IntentType::kDictionary, intent_info_.intent_type);
  EXPECT_EQ(kWord, intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest,
       ShouldNotTriggerForSingleWordInDictionaryWithDigits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::features::kQuickAnswersAlwaysTriggerForSingleWord);

  const std::string kWord = "1st";

  // No Annotation provided.
  std::vector<TextAnnotationPtr> annotations;
  UseFakeServiceConnection(annotations);

  // Add word to the dictionary.
  spell_checker()->AddWordToDictionary(kWord);

  // Word selected.
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = kWord;

  intent_generator_->GenerateIntent(*quick_answers_request);
  task_environment_.RunUntilIdle();

  // Should not generate dictionary intent if the word contains digits even if
  // it is in the dictionary.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ(kWord, intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, ShouldNotTriggerForProperNounInDictionary) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::features::kQuickAnswersAlwaysTriggerForSingleWord);

  const std::string kWord = "Amy";

  // No Annotation provided.
  std::vector<TextAnnotationPtr> annotations;
  UseFakeServiceConnection(annotations);

  // Add word to the dictionary.
  spell_checker()->AddWordToDictionary(kWord);

  // Word selected.
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = kWord;

  intent_generator_->GenerateIntent(*quick_answers_request);
  task_environment_.RunUntilIdle();

  // Should not generate dictionary intent if the word contains digits even if
  // it is in the dictionary.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ(kWord, intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest,
       ShouldFallbackToAnnotationsForWordNotInDictionaryNoAnnotation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::features::kQuickAnswersAlwaysTriggerForSingleWord);

  const std::string kWord = "single";

  // No Annotation provided, and not add the word to the dictionary.
  std::vector<TextAnnotationPtr> annotations;
  UseFakeServiceConnection(annotations);

  // Word selected.
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = kWord;

  intent_generator_->GenerateIntent(*quick_answers_request);
  task_environment_.RunUntilIdle();

  // Should not generate dictionary intent if the word is not in the dictionary
  // and no annotation provided.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ(kWord, intent_info_.intent_text);
}

TEST_F(
    IntentGeneratorTest,
    ShouldFallbackToAnnotationsForWordNotInDictionaryWithDictionaryAnnotation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::features::kQuickAnswersAlwaysTriggerForSingleWord);

  const std::string kWord = "unfathomable";

  // Annotation provided, and not add the word to the dictionary.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(TextEntity::New(
      "dictionary",                            // Entity name.
      1.0,                                     // Confidence score.
      TextEntityData::NewNumericValue(0.0)));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,   // Start offset.
                                                   12,  // End offset.
                                                   std::move(entities));
  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());
  UseFakeServiceConnection(annotations);

  // Word selected.
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = kWord;

  intent_generator_->GenerateIntent(*quick_answers_request);
  task_environment_.RunUntilIdle();

  // Should generate dictionary intent for the word.
  EXPECT_EQ(IntentType::kDictionary, intent_info_.intent_type);
  EXPECT_EQ(kWord, intent_info_.intent_text);
}

TEST_F(
    IntentGeneratorTest,
    ShouldFallbackToAnnotationsForWordNotInDictionaryWithUnitConversionAnnotation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::features::kQuickAnswersAlwaysTriggerForSingleWord);

  const std::string kText = "50kg";

  // Annotation provided, and not add the text to the dictionary.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(TextEntity::New(
      "unit",                                  // Entity name.
      1.0,                                     // Confidence score.
      TextEntityData::NewNumericValue(0.0)));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,  // Start offset.
                                                   4,  // End offset.
                                                   std::move(entities));
  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());
  UseFakeServiceConnection(annotations);

  // Text selected.
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = kText;

  intent_generator_->GenerateIntent(*quick_answers_request);
  task_environment_.RunUntilIdle();

  // Should generate unit conversion intent.
  EXPECT_EQ(IntentType::kUnit, intent_info_.intent_type);
  EXPECT_EQ(kText, intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, ShouldNotTriggerForMultipleWords) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      chromeos::features::kQuickAnswersAlwaysTriggerForSingleWord);

  // No Annotation provided.
  std::vector<TextAnnotationPtr> annotations;
  UseFakeServiceConnection(annotations);

  // Multiple words selected.
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "multiple words";

  intent_generator_->GenerateIntent(*quick_answers_request);
  task_environment_.RunUntilIdle();

  // Should fallback to unknown intent.
  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("multiple words", intent_info_.intent_text);
}

}  // namespace quick_answers
