// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/handwriting_recognizer.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/tensor.mojom.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace machine_learning {
namespace {

class ServiceConnectionTest : public testing::Test {
 public:
  ServiceConnectionTest() = default;

  void SetUp() override { MachineLearningClient::InitializeFake(); }

  void TearDown() override { MachineLearningClient::Shutdown(); }

 protected:
  static void SetUpTestCase() {
    static base::Thread ipc_thread("ipc");
    ipc_thread.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    static mojo::core::ScopedIPCSupport ipc_support(
        ipc_thread.task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ServiceConnectionTest);
};

// Tests that LoadBuiltinModel runs OK (no crash) in a basic Mojo
// environment.
TEST_F(ServiceConnectionTest, LoadBuiltinModel) {
  ServiceConnection::GetInstance()->Initialize();
  mojo::Remote<mojom::Model> model;

  mojo::Remote<mojom::MachineLearningService> ml_service;
  ServiceConnection::GetInstance()->BindMachineLearningService(
      ml_service.BindNewPipeAndPassReceiver());

  ml_service->LoadBuiltinModel(
      mojom::BuiltinModelSpec::New(mojom::BuiltinModelId::TEST_MODEL),
      model.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::LoadModelResult result) {}));

  // Also tests GetMachineLearningService runs OK.
  model.reset();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadBuiltinModel(
          mojom::BuiltinModelSpec::New(mojom::BuiltinModelId::TEST_MODEL),
          model.BindNewPipeAndPassReceiver(),
          base::BindOnce([](mojom::LoadModelResult result) {}));
}

// Tests that LoadFlatBufferModel runs OK (no crash) in a basic Mojo
// environment.
TEST_F(ServiceConnectionTest, LoadFlatBufferModel) {
  ServiceConnection::GetInstance()->Initialize();

  mojo::Remote<mojom::MachineLearningService> ml_service;
  ServiceConnection::GetInstance()->BindMachineLearningService(
      ml_service.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::Model> model;
  ml_service->LoadFlatBufferModel(
      mojom::FlatBufferModelSpec::New(), model.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::LoadModelResult result) {}));

  model.reset();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadFlatBufferModel(
          mojom::FlatBufferModelSpec::New(), model.BindNewPipeAndPassReceiver(),
          base::BindOnce([](mojom::LoadModelResult result) {}));
}

// Tests that LoadTextClassifier runs OK (no crash) in a basic Mojo
// environment.
TEST_F(ServiceConnectionTest, LoadTextClassifier) {
  ServiceConnection::GetInstance()->Initialize();

  mojo::Remote<mojom::MachineLearningService> ml_service;
  ServiceConnection::GetInstance()->BindMachineLearningService(
      ml_service.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::TextClassifier> text_classifier;
  ml_service->LoadTextClassifier(
      text_classifier.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::LoadModelResult result) {}));

  text_classifier.reset();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadTextClassifier(text_classifier.BindNewPipeAndPassReceiver(),
                          base::BindOnce([](mojom::LoadModelResult result) {}));
}

// Tests that LoadHandwritingModelWithSpec runs OK (no crash) in a basic Mojo
// environment.
TEST_F(ServiceConnectionTest, LoadHandwritingModelWithSpec) {
  ServiceConnection::GetInstance()->Initialize();

  mojo::Remote<mojom::MachineLearningService> ml_service;
  ServiceConnection::GetInstance()->BindMachineLearningService(
      ml_service.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::HandwritingRecognizer> handwriting_recognizer;
  ml_service->LoadHandwritingModelWithSpec(
      mojom::HandwritingRecognizerSpec::New("en"),
      handwriting_recognizer.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::LoadModelResult result) {}));

  handwriting_recognizer.reset();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadHandwritingModelWithSpec(
          mojom::HandwritingRecognizerSpec::New("en"),
          handwriting_recognizer.BindNewPipeAndPassReceiver(),
          base::BindOnce([](mojom::LoadModelResult result) {}));
}

// Tests that LoadGrammarChecker runs OK (no crash) in a basic Mojo environment.
TEST_F(ServiceConnectionTest, LoadGrammarModel) {
  ServiceConnection::GetInstance()->Initialize();

  mojo::Remote<mojom::MachineLearningService> ml_service;
  ServiceConnection::GetInstance()->BindMachineLearningService(
      ml_service.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::GrammarChecker> grammar_checker;
  ml_service->LoadGrammarChecker(
      grammar_checker.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::LoadModelResult result) {}));

  grammar_checker.reset();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadGrammarChecker(grammar_checker.BindNewPipeAndPassReceiver(),
                          base::BindOnce([](mojom::LoadModelResult result) {}));
}

// Tests the fake ML service for binding ml_service receiver.
TEST_F(ServiceConnectionTest, BindMachineLearningService) {
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  mojo::Remote<mojom::MachineLearningService> ml_service;
  ServiceConnection::GetInstance()->BindMachineLearningService(
      ml_service.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(ml_service.is_bound());

  // Check the bound ml_service remote can be used to call
  // MachineLearningService methods.
  mojo::Remote<mojom::Model> model;
  bool callback_done = false;

  ml_service->LoadBuiltinModel(
      mojom::BuiltinModelSpec::New(mojom::BuiltinModelId::TEST_MODEL),
      model.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* callback_done, mojom::LoadModelResult result) {
            EXPECT_EQ(result, mojom::LoadModelResult::OK);
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_done);
  EXPECT_TRUE(model.is_bound());
}

class TestSodaClient : public mojom::SodaClient {
  void OnStop() override {}
  void OnStart() override {}
  void OnSpeechRecognizerEvent(mojom::SpeechRecognizerEventPtr event) override {
  }
};

// Tests that LoadSpeechRecognizer runs OK without a crash in a basic Mojo
// Environment.
TEST_F(ServiceConnectionTest, LoadSpeechRecognizerAndCallback) {
  mojo::Remote<mojom::SodaRecognizer> soda_recognizer;
  TestSodaClient test_client;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  mojo::Receiver<mojom::SodaClient> soda_client{&test_client};
  bool callback_done = false;
  auto config = mojom::SodaConfig::New();
  base::RunLoop run_loop;
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadSpeechRecognizer(
          std::move(config), soda_client.BindNewPipeAndPassRemote(),
          soda_recognizer.BindNewPipeAndPassReceiver(),
          base::BindLambdaForTesting([&](mojom::LoadModelResult result) {
            callback_done = true;
            EXPECT_EQ(result, mojom::LoadModelResult::OK);
            run_loop.Quit();
          }));
  run_loop.Run();
  ASSERT_TRUE(callback_done);
}

// Tests the fake ML service for builtin model.
TEST_F(ServiceConnectionTest, FakeServiceConnectionForBuiltinModel) {
  mojo::Remote<mojom::Model> model;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  const double expected_value = 200.002;
  fake_service_connection.SetOutputValue(std::vector<int64_t>{1L},
                                         std::vector<double>{expected_value});
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadBuiltinModel(
          mojom::BuiltinModelSpec::New(mojom::BuiltinModelId::TEST_MODEL),
          model.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(model.is_bound());

  callback_done = false;
  mojo::Remote<mojom::GraphExecutor> graph;
  model->CreateGraphExecutor(
      graph.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* callback_done, mojom::CreateGraphExecutorResult result) {
            EXPECT_EQ(result, mojom::CreateGraphExecutorResult::OK);
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(graph.is_bound());

  callback_done = false;
  base::flat_map<std::string, mojom::TensorPtr> inputs;
  std::vector<std::string> outputs;
  graph->Execute(std::move(inputs), std::move(outputs),
                 base::BindOnce(
                     [](bool* callback_done, double expected_value,
                        const mojom::ExecuteResult result,
                        base::Optional<std::vector<mojom::TensorPtr>> outputs) {
                       EXPECT_EQ(result, mojom::ExecuteResult::OK);
                       ASSERT_TRUE(outputs.has_value());
                       ASSERT_EQ(outputs->size(), 1LU);
                       mojom::TensorPtr& tensor = (*outputs)[0];
                       EXPECT_EQ(tensor->data->get_float_list()->value[0],
                                 expected_value);

                       *callback_done = true;
                     },
                     &callback_done, expected_value));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
}

// Tests the fake ML service for flatbuffer model.
TEST_F(ServiceConnectionTest, FakeServiceConnectionForFlatBufferModel) {
  mojo::Remote<mojom::Model> model;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  const double expected_value = 200.002;
  fake_service_connection.SetOutputValue(std::vector<int64_t>{1L},
                                         std::vector<double>{expected_value});

  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadFlatBufferModel(
          mojom::FlatBufferModelSpec::New(), model.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(model.is_bound());

  callback_done = false;
  mojo::Remote<mojom::GraphExecutor> graph;
  model->CreateGraphExecutor(
      graph.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* callback_done, mojom::CreateGraphExecutorResult result) {
            EXPECT_EQ(result, mojom::CreateGraphExecutorResult::OK);
            *callback_done = true;
          },
          &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(graph.is_bound());

  callback_done = false;
  base::flat_map<std::string, mojom::TensorPtr> inputs;
  std::vector<std::string> outputs;
  graph->Execute(std::move(inputs), std::move(outputs),
                 base::BindOnce(
                     [](bool* callback_done, double expected_value,
                        const mojom::ExecuteResult result,
                        base::Optional<std::vector<mojom::TensorPtr>> outputs) {
                       EXPECT_EQ(result, mojom::ExecuteResult::OK);
                       ASSERT_TRUE(outputs.has_value());
                       ASSERT_EQ(outputs->size(), 1LU);
                       mojom::TensorPtr& tensor = (*outputs)[0];
                       EXPECT_EQ(tensor->data->get_float_list()->value[0],
                                 expected_value);

                       *callback_done = true;
                     },
                     &callback_done, expected_value));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
}

// Tests the fake ML service for text classifier annotation.
TEST_F(ServiceConnectionTest,
       FakeServiceConnectionForTextClassifierAnnotation) {
  mojo::Remote<mojom::TextClassifier> text_classifier;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  auto dummy_data = mojom::TextEntityData::New();
  dummy_data->set_numeric_value(123456789.);
  std::vector<mojom::TextEntityPtr> entities;
  entities.emplace_back(
      mojom::TextEntity::New("dummy",                      // Entity name.
                             1.0,                          // Confidence score.
                             std::move(dummy_data)));      // Data extracted.
  auto dummy_annotation = mojom::TextAnnotation::New(123,  // Start offset.
                                                     321,  // End offset.
                                                     std::move(entities));
  std::vector<mojom::TextAnnotationPtr> annotations;
  annotations.emplace_back(std::move(dummy_annotation));
  fake_service_connection.SetOutputAnnotation(annotations);

  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadTextClassifier(
          text_classifier.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(text_classifier.is_bound());

  auto request = mojom::TextAnnotationRequest::New();
  bool infer_callback_done = false;
  text_classifier->Annotate(
      std::move(request),
      base::BindOnce(
          [](bool* infer_callback_done,
             std::vector<mojom::TextAnnotationPtr> annotations) {
            *infer_callback_done = true;
            // Check if the annotation is correct.
            EXPECT_EQ(annotations[0]->start_offset, 123u);
            EXPECT_EQ(annotations[0]->end_offset, 321u);
            EXPECT_EQ(annotations[0]->entities[0]->name, "dummy");
            EXPECT_EQ(annotations[0]->entities[0]->confidence_score, 1.0);
            EXPECT_EQ(annotations[0]->entities[0]->data->get_numeric_value(),
                      123456789.);
          },
          &infer_callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(infer_callback_done);
}

// Tests the fake ML service for text classifier suggest selection.
TEST_F(ServiceConnectionTest,
       FakeServiceConnectionForTextClassifierSuggestSelection) {
  mojo::Remote<mojom::TextClassifier> text_classifier;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  auto span = mojom::CodepointSpan::New();
  span->start_offset = 1;
  span->end_offset = 2;
  fake_service_connection.SetOutputSelection(span);

  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadTextClassifier(
          text_classifier.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(text_classifier.is_bound());

  auto request = mojom::TextSuggestSelectionRequest::New();
  request->user_selection = mojom::CodepointSpan::New();
  bool infer_callback_done = false;
  text_classifier->SuggestSelection(
      std::move(request), base::BindOnce(
                              [](bool* infer_callback_done,
                                 mojom::CodepointSpanPtr suggested_span) {
                                *infer_callback_done = true;
                                // Check if the suggestion is correct.
                                EXPECT_EQ(suggested_span->start_offset, 1u);
                                EXPECT_EQ(suggested_span->end_offset, 2u);
                              },
                              &infer_callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(infer_callback_done);
}

// Tests the fake ML service for text classifier language identification.
TEST_F(ServiceConnectionTest,
       FakeServiceConnectionForTextClassifierFindLanguages) {
  mojo::Remote<mojom::TextClassifier> text_classifier;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  std::vector<mojom::TextLanguagePtr> languages;
  languages.emplace_back(mojom::TextLanguage::New("en", 0.9));
  languages.emplace_back(mojom::TextLanguage::New("fr", 0.1));
  fake_service_connection.SetOutputLanguages(languages);

  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadTextClassifier(
          text_classifier.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(text_classifier.is_bound());

  std::string input_text = "dummy input text";
  bool infer_callback_done = false;
  text_classifier->FindLanguages(
      input_text, base::BindOnce(
                      [](bool* infer_callback_done,
                         std::vector<mojom::TextLanguagePtr> languages) {
                        *infer_callback_done = true;
                        // Check if the suggestion is correct.
                        ASSERT_EQ(languages.size(), 2ul);
                        EXPECT_EQ(languages[0]->locale, "en");
                        EXPECT_EQ(languages[0]->confidence, 0.9f);
                        EXPECT_EQ(languages[1]->locale, "fr");
                        EXPECT_EQ(languages[1]->confidence, 0.1f);
                      },
                      &infer_callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(infer_callback_done);
}

// Tests the fake ML service for handwriting.
TEST_F(ServiceConnectionTest, FakeHandWritingRecognizer) {
  mojo::Remote<mojom::HandwritingRecognizer> recognizer;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadHandwritingModel(
          mojom::HandwritingRecognizerSpec::New("en"),
          recognizer.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done,
                 mojom::LoadHandwritingModelResult result) {
                EXPECT_EQ(result, mojom::LoadHandwritingModelResult::OK);
                *callback_done = true;
              },
              &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(recognizer.is_bound());

  // Construct fake output.
  mojom::HandwritingRecognizerResultPtr result =
      mojom::HandwritingRecognizerResult::New();
  result->status = mojom::HandwritingRecognizerResult::Status::OK;
  mojom::HandwritingRecognizerCandidatePtr candidate =
      mojom::HandwritingRecognizerCandidate::New();
  candidate->text = "cat";
  candidate->score = 0.5f;
  result->candidates.emplace_back(std::move(candidate));
  fake_service_connection.SetOutputHandwritingRecognizerResult(result);

  auto query = mojom::HandwritingRecognitionQuery::New();
  bool infer_callback_done = false;
  recognizer->Recognize(
      std::move(query),
      base::BindOnce(
          [](bool* infer_callback_done,
             mojom::HandwritingRecognizerResultPtr result) {
            *infer_callback_done = true;
            // Check if the annotation is correct.
            ASSERT_EQ(result->status,
                      mojom::HandwritingRecognizerResult::Status::OK);
            EXPECT_EQ(result->candidates.at(0)->text, "cat");
            EXPECT_EQ(result->candidates.at(0)->score, 0.5f);
          },
          &infer_callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(infer_callback_done);
}

// Tests the deprecated fake ML service for handwriting.
// Deprecated API.
TEST_F(ServiceConnectionTest, FakeHandWritingRecognizerWithSpec) {
  mojo::Remote<mojom::HandwritingRecognizer> recognizer;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadHandwritingModelWithSpec(
          mojom::HandwritingRecognizerSpec::New("en"),
          recognizer.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(recognizer.is_bound());

  // Construct fake output.
  mojom::HandwritingRecognizerResultPtr result =
      mojom::HandwritingRecognizerResult::New();
  result->status = mojom::HandwritingRecognizerResult::Status::OK;
  mojom::HandwritingRecognizerCandidatePtr candidate =
      mojom::HandwritingRecognizerCandidate::New();
  candidate->text = "cat";
  candidate->score = 0.5f;
  result->candidates.emplace_back(std::move(candidate));
  fake_service_connection.SetOutputHandwritingRecognizerResult(result);

  auto query = mojom::HandwritingRecognitionQuery::New();
  bool infer_callback_done = false;
  recognizer->Recognize(
      std::move(query),
      base::BindOnce(
          [](bool* infer_callback_done,
             mojom::HandwritingRecognizerResultPtr result) {
            *infer_callback_done = true;
            // Check if the annotation is correct.
            ASSERT_EQ(result->status,
                      mojom::HandwritingRecognizerResult::Status::OK);
            EXPECT_EQ(result->candidates.at(0)->text, "cat");
            EXPECT_EQ(result->candidates.at(0)->score, 0.5f);
          },
          &infer_callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(infer_callback_done);
}

TEST_F(ServiceConnectionTest, FakeGrammarChecker) {
  mojo::Remote<mojom::GrammarChecker> checker;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadGrammarChecker(
          checker.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(checker.is_bound());

  // Construct fake output
  mojom::GrammarCheckerResultPtr result = mojom::GrammarCheckerResult::New();
  result->status = mojom::GrammarCheckerResult::Status::OK;
  mojom::GrammarCheckerCandidatePtr candidate =
      mojom::GrammarCheckerCandidate::New();
  candidate->text = "cat";
  candidate->score = 0.5f;
  mojom::GrammarCorrectionFragmentPtr fragment =
      mojom::GrammarCorrectionFragment::New();
  fragment->offset = 3;
  fragment->length = 5;
  fragment->replacement = "dog";
  candidate->fragments.emplace_back(std::move(fragment));
  result->candidates.emplace_back(std::move(candidate));
  fake_service_connection.SetOutputGrammarCheckerResult(result);

  auto query = mojom::GrammarCheckerQuery::New();
  bool infer_callback_done = false;
  checker->Check(
      std::move(query),
      base::BindOnce(
          [](bool* infer_callback_done, mojom::GrammarCheckerResultPtr result) {
            *infer_callback_done = true;
            // Check if the annotation is correct.
            ASSERT_EQ(result->status, mojom::GrammarCheckerResult::Status::OK);
            ASSERT_EQ(result->candidates.size(), 1UL);
            EXPECT_EQ(result->candidates.at(0)->text, "cat");
            EXPECT_EQ(result->candidates.at(0)->score, 0.5f);

            ASSERT_EQ(result->candidates.at(0)->fragments.size(), 1UL);
            EXPECT_EQ(result->candidates.at(0)->fragments.at(0)->offset, 3U);
            EXPECT_EQ(result->candidates.at(0)->fragments.at(0)->length, 5U);
            EXPECT_EQ(result->candidates.at(0)->fragments.at(0)->replacement,
                      "dog");
          },
          &infer_callback_done));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(infer_callback_done);
}

}  // namespace
}  // namespace machine_learning
}  // namespace chromeos
