// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/understanding/intent_generator.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/text_classifier.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {
namespace {

using chromeos::machine_learning::FakeServiceConnectionImpl;
using machine_learning::mojom::TextAnnotation;
using machine_learning::mojom::TextAnnotationPtr;
using machine_learning::mojom::TextEntity;
using machine_learning::mojom::TextEntityData;
using machine_learning::mojom::TextEntityPtr;
using machine_learning::mojom::TextLanguage;
using machine_learning::mojom::TextLanguagePtr;

TextLanguagePtr DefaultLanguage() {
  return TextLanguage::New("en", /* confidence */ 1);
}

}  // namespace

class IntentGeneratorTest : public testing::Test {
 public:
  IntentGeneratorTest() = default;

  IntentGeneratorTest(const IntentGeneratorTest&) = delete;
  IntentGeneratorTest& operator=(const IntentGeneratorTest&) = delete;

  void SetUp() override {
    intent_generator_ = std::make_unique<IntentGenerator>(
        base::BindOnce(&IntentGeneratorTest::IntentGeneratorTestCallback,
                       base::Unretained(this)));

    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kQuickAnswersTextAnnotator,
         chromeos::features::kQuickAnswersTranslation},
        {});
  }

  void TearDown() override { intent_generator_.reset(); }

  void IntentGeneratorTestCallback(const IntentInfo& intent_info) {
    intent_info_ = intent_info;
  }

 protected:
  void UseFakeServiceConnection(
      const std::vector<TextAnnotationPtr>& annotations =
          std::vector<TextAnnotationPtr>(),
      const std::vector<TextLanguagePtr>& languages =
          std::vector<TextLanguagePtr>()) {
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    fake_service_connection_.SetOutputAnnotation(annotations);
    fake_service_connection_.SetOutputLanguages(languages);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<IntentGenerator> intent_generator_;
  IntentInfo intent_info_;
  base::test::ScopedFeatureList scoped_feature_list_;
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_service_connection_;
};

TEST_F(IntentGeneratorTest, TranslationIntent) {
  std::vector<TextLanguagePtr> languages;
  languages.push_back(DefaultLanguage());
  UseFakeServiceConnection({}, languages);

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  request.context.device_properties.language = "es";
  intent_generator_->GenerateIntent(request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kTranslation, intent_info_.intent_type);
  EXPECT_EQ("quick answers", intent_info_.intent_text);
  EXPECT_EQ("en", intent_info_.source_language);
  EXPECT_EQ("es", intent_info_.target_language);
}

TEST_F(IntentGeneratorTest, TranslationIntentSameLanguage) {
  std::vector<TextLanguagePtr> languages;
  languages.push_back(DefaultLanguage());
  UseFakeServiceConnection({}, languages);

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  request.context.device_properties.language = "en";
  intent_generator_->GenerateIntent(request);

  task_environment_.RunUntilIdle();

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
      "more.";
  request.context.device_properties.language = "es";
  intent_generator_->GenerateIntent(request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ(
      "Search the world's information, including webpages, images, videos and "
      "more.",
      intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TranslationIntentNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {chromeos::features::kQuickAnswersTextAnnotator},
      {chromeos::features::kQuickAnswersTranslation});
  std::vector<TextLanguagePtr> languages;
  languages.push_back(DefaultLanguage());
  UseFakeServiceConnection({}, languages);

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  request.context.device_properties.language = "es";
  intent_generator_->GenerateIntent(request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("quick answers", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TranslationIntentDeviceLanguageNotSet) {
  std::vector<TextLanguagePtr> languages;
  languages.push_back(DefaultLanguage());
  UseFakeServiceConnection({}, languages);

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  intent_generator_->GenerateIntent(request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("quick answers", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationDefinitionIntent) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "unfathomable";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(
      TextEntity::New("dictionary",             // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,   // Start offset.
                                                   12,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  task_environment_.RunUntilIdle();

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
  entities.emplace_back(
      TextEntity::New("dictionary",             // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(1,   // Start offset.
                                                   13,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  task_environment_.RunUntilIdle();

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
  entities.emplace_back(
      TextEntity::New("dictionary",             // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(4,   // Start offset.
                                                   16,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("unfathomable", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationUnitIntentExtraChars) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "23 cm to";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(
      TextEntity::New("unit",                   // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,  // Start offset.
                                                   5,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnit, intent_info_.intent_type);
  EXPECT_EQ("23 cm", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationUnitIntentUtf16Char) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "350°F";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(
      TextEntity::New("unit",                   // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,  // Start offset.
                                                   5,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnit, intent_info_.intent_type);
  EXPECT_EQ("350°F", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationUnitIntentExtraCharsAboveThreshold) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "23 cm is equal to 9.06 inches";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(
      TextEntity::New("unit",                   // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,  // Start offset.
                                                   5,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  task_environment_.RunUntilIdle();

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
  task_environment_.RunUntilIdle();

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
  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("the unfathomable reaches of space", intent_info_.intent_text);
}

TEST_F(IntentGeneratorTest, TextAnnotationIntentUnSupportedEntity) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "the unfathomable reaches of space";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(
      TextEntity::New("something_else",         // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto some_annotation = TextAnnotation::New(4,   // Start offset.
                                             16,  // End offset.
                                             std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(some_annotation->Clone());
  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_info_.intent_type);
  EXPECT_EQ("the unfathomable reaches of space", intent_info_.intent_text);
}
}  // namespace quick_answers
}  // namespace chromeos
