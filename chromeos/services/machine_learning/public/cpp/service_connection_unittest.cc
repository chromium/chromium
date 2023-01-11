// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
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
#include "chromeos/services/machine_learning/public/mojom/text_suggester.mojom.h"
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

  ServiceConnectionTest(const ServiceConnectionTest&) = delete;
  ServiceConnectionTest& operator=(const ServiceConnectionTest&) = delete;

  void SetUp() override { MachineLearningClient::InitializeFake(); }

  void TearDown() override { MachineLearningClient::Shutdown(); }

 protected:
  static void SetUpTestCase() {
    task_environment_ = new base::test::TaskEnvironment();
    static base::Thread ipc_thread("ipc");
    ipc_thread.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    static mojo::core::ScopedIPCSupport ipc_support(
        ipc_thread.task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
    ServiceConnection::GetInstance()->Initialize();
  }

  static void TearDownTestCase() {
    if (task_environment_) {
      delete task_environment_;
      task_environment_ = nullptr;
    }
  }

 private:
  static base::test::TaskEnvironment* task_environment_;
};

base::test::TaskEnvironment* ServiceConnectionTest::task_environment_;

// Tests that LoadBuiltinModel runs OK (no crash) in a basic Mojo
// environment.
TEST_F(ServiceConnectionTest, LoadBuiltinModel) {
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

// Tests that LoadHandwritingModel runs OK (no crash) in a basic Mojo
// environment.
TEST_F(ServiceConnectionTest, LoadHandwritingModel) {
  mojo::Remote<mojom::MachineLearningService> ml_service;
  ServiceConnection::GetInstance()->BindMachineLearningService(
      ml_service.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::HandwritingRecognizer> handwriting_recognizer;
  ml_service->LoadHandwritingModel(
      mojom::HandwritingRecognizerSpec::New("en"),
      handwriting_recognizer.BindNewPipeAndPassReceiver(),
      base::BindOnce([](mojom::LoadHandwritingModelResult result) {}));

  handwriting_recognizer.reset();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadHandwritingModel(
          mojom::HandwritingRecognizerSpec::New("en"),
          handwriting_recognizer.BindNewPipeAndPassReceiver(),
          base::BindOnce([](mojom::LoadHandwritingModelResult result) {}));
}

// Tests that LoadGrammarChecker runs OK (no crash) in a basic Mojo environment.
TEST_F(ServiceConnectionTest, LoadGrammarModel) {
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

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  mojo::Remote<mojom::MachineLearningService> ml_service;
  base::OnceClosure callback =
      base::BindOnce(&ServiceConnection::BindMachineLearningService,
                     base::Unretained(ServiceConnection::GetInstance()),
                     ml_service.BindNewPipeAndPassReceiver())
          .Then(run_loop->QuitClosure());
  std::move(callback).Run();
  run_loop->Run();
  ASSERT_TRUE(ml_service.is_bound());

  // Check the bound ml_service remote can be used to call
  // MachineLearningService methods.
  mojo::Remote<mojom::Model> model;
  bool callback_done = false;

  run_loop.reset(new base::RunLoop);
  ml_service->LoadBuiltinModel(
      mojom::BuiltinModelSpec::New(mojom::BuiltinModelId::TEST_MODEL),
      model.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* callback_done, mojom::LoadModelResult result) {
            EXPECT_EQ(result, mojom::LoadModelResult::OK);
            *callback_done = true;
          },
          &callback_done)
          .Then(run_loop->QuitClosure()));

  run_loop->Run();
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
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
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
              &callback_done)
              .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(model.is_bound());

  callback_done = false;
  mojo::Remote<mojom::GraphExecutor> graph;
  run_loop.reset(new base::RunLoop);
  model->CreateGraphExecutor(
      mojom::GraphExecutorOptions::New(), graph.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* callback_done, mojom::CreateGraphExecutorResult result) {
            EXPECT_EQ(result, mojom::CreateGraphExecutorResult::OK);
            *callback_done = true;
          },
          &callback_done)
          .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(graph.is_bound());

  callback_done = false;
  base::flat_map<std::string, mojom::TensorPtr> inputs;
  std::vector<std::string> outputs;
  run_loop.reset(new base::RunLoop);
  graph->Execute(std::move(inputs), std::move(outputs),
                 base::BindOnce(
                     [](bool* callback_done, double expected_value,
                        const mojom::ExecuteResult result,
                        absl::optional<std::vector<mojom::TensorPtr>> outputs) {
                       EXPECT_EQ(result, mojom::ExecuteResult::OK);
                       ASSERT_TRUE(outputs.has_value());
                       ASSERT_EQ(outputs->size(), 1LU);
                       mojom::TensorPtr& tensor = (*outputs)[0];
                       EXPECT_EQ(tensor->data->get_float_list()->value[0],
                                 expected_value);

                       *callback_done = true;
                     },
                     &callback_done, expected_value)
                     .Then(run_loop->QuitClosure()));

  run_loop->Run();
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

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadFlatBufferModel(
          mojom::FlatBufferModelSpec::New(), model.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done)
              .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(model.is_bound());

  callback_done = false;
  mojo::Remote<mojom::GraphExecutor> graph;
  run_loop.reset(new base::RunLoop);
  model->CreateGraphExecutor(
      mojom::GraphExecutorOptions::New(), graph.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* callback_done, mojom::CreateGraphExecutorResult result) {
            EXPECT_EQ(result, mojom::CreateGraphExecutorResult::OK);
            *callback_done = true;
          },
          &callback_done)
          .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(graph.is_bound());

  callback_done = false;
  base::flat_map<std::string, mojom::TensorPtr> inputs;
  std::vector<std::string> outputs;
  run_loop.reset(new base::RunLoop);
  graph->Execute(std::move(inputs), std::move(outputs),
                 base::BindOnce(
                     [](bool* callback_done, double expected_value,
                        const mojom::ExecuteResult result,
                        absl::optional<std::vector<mojom::TensorPtr>> outputs) {
                       EXPECT_EQ(result, mojom::ExecuteResult::OK);
                       ASSERT_TRUE(outputs.has_value());
                       ASSERT_EQ(outputs->size(), 1LU);
                       mojom::TensorPtr& tensor = (*outputs)[0];
                       EXPECT_EQ(tensor->data->get_float_list()->value[0],
                                 expected_value);

                       *callback_done = true;
                     },
                     &callback_done, expected_value)
                     .Then(run_loop->QuitClosure()));
  run_loop->Run();
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

  auto dummy_data = mojom::TextEntityData::NewNumericValue(123456789.);
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

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadTextClassifier(
          text_classifier.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done)
              .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(text_classifier.is_bound());

  auto request = mojom::TextAnnotationRequest::New();
  bool infer_callback_done = false;
  run_loop.reset(new base::RunLoop);
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
          &infer_callback_done)
          .Then(run_loop->QuitClosure()));
  run_loop->Run();
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

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadTextClassifier(
          text_classifier.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done)
              .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(text_classifier.is_bound());

  std::string input_text = "dummy input text";
  bool infer_callback_done = false;
  run_loop.reset(new base::RunLoop);
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
                      &infer_callback_done)
                      .Then(run_loop->QuitClosure()));
  run_loop->Run();
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

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
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
              &callback_done)
              .Then(run_loop->QuitClosure()));
  run_loop->Run();
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
  run_loop.reset(new base::RunLoop);
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
          &infer_callback_done)
          .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(infer_callback_done);
}

// Tests the fake ML service for web platform handwriting recognizer.
TEST_F(ServiceConnectionTest, FakeWebPlatformHandWritingRecognizer) {
  mojo::Remote<web_platform::mojom::HandwritingRecognizer> recognizer;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();
  auto constraint = web_platform::mojom::HandwritingModelConstraint::New();
  constraint->languages.emplace_back("en");
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadWebPlatformHandwritingModel(
          std::move(constraint), recognizer.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done,
                 mojom::LoadHandwritingModelResult result) {
                EXPECT_EQ(result, mojom::LoadHandwritingModelResult::OK);
                *callback_done = true;
              },
              &callback_done)
              .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(recognizer.is_bound());

  // Construct fake output.
  std::vector<web_platform::mojom::HandwritingPredictionPtr> predictions;
  auto prediction1 = web_platform::mojom::HandwritingPrediction::New();
  prediction1->text = "recognition1";
  predictions.emplace_back(std::move(prediction1));
  fake_service_connection.SetOutputWebPlatformHandwritingRecognizerResult(
      predictions);

  std::vector<web_platform::mojom::HandwritingStrokePtr> strokes;
  auto hints = web_platform::mojom::HandwritingHints::New();
  bool infer_callback_done = false;
  run_loop.reset(new base::RunLoop);
  recognizer->GetPrediction(
      std::move(strokes), std::move(hints),
      base::BindOnce(
          [](bool* infer_callback_done,
             absl::optional<std::vector<
                 web_platform::mojom::HandwritingPredictionPtr>> predictions) {
            *infer_callback_done = true;
            ASSERT_TRUE(predictions.has_value());
            ASSERT_EQ(predictions.value().size(), 1u);
          },
          &infer_callback_done)
          .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(infer_callback_done);
}

TEST_F(ServiceConnectionTest, FakeGrammarChecker) {
  mojo::Remote<mojom::GrammarChecker> checker;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadGrammarChecker(
          checker.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done)
              .Then(run_loop->QuitClosure()));
  run_loop->Run();
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
  run_loop.reset(new base::RunLoop);
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
          &infer_callback_done)
          .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(infer_callback_done);
}

TEST_F(ServiceConnectionTest, FakeTextSuggester) {
  mojo::Remote<mojom::TextSuggester> suggester;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadTextSuggester(
          suggester.BindNewPipeAndPassReceiver(),
          mojom::TextSuggesterSpec::New(),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done)
              .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(suggester.is_bound());

  // Construct fake output
  mojom::TextSuggesterResultPtr result = mojom::TextSuggesterResult::New();
  result->status = mojom::TextSuggesterResult::Status::OK;

  mojom::MultiWordSuggestionCandidatePtr multi_word =
      mojom::MultiWordSuggestionCandidate::New();
  multi_word->text = "hello";
  multi_word->normalized_score = 0.5f;
  mojom::TextSuggestionCandidatePtr candidate =
      mojom::TextSuggestionCandidate::NewMultiWord(std::move(multi_word));

  result->candidates.emplace_back(std::move(candidate));
  fake_service_connection.SetOutputTextSuggesterResult(result);

  auto query = mojom::TextSuggesterQuery::New();
  bool infer_callback_done = false;
  run_loop.reset(new base::RunLoop);
  suggester->Suggest(
      std::move(query),
      base::BindOnce(
          [](bool* infer_callback_done, mojom::TextSuggesterResultPtr result) {
            *infer_callback_done = true;
            // Check the fake suggestion is returned
            ASSERT_EQ(result->status, mojom::TextSuggesterResult::Status::OK);
            ASSERT_EQ(result->candidates.size(), 1UL);
            ASSERT_TRUE(result->candidates.at(0)->is_multi_word());
            EXPECT_EQ(result->candidates.at(0)->get_multi_word()->text,
                      "hello");
            EXPECT_EQ(
                result->candidates.at(0)->get_multi_word()->normalized_score,
                0.5f);
          },
          &infer_callback_done)
          .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(infer_callback_done);
}

// Tests the fake ML service for document scanner.
TEST_F(ServiceConnectionTest, FakeDocumentScanner) {
  mojo::Remote<mojom::DocumentScanner> scanner;
  bool callback_done = false;
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  auto config = chromeos::machine_learning::mojom::DocumentScannerConfig::New();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadDocumentScanner(
          scanner.BindNewPipeAndPassReceiver(), std::move(config),
          base::BindOnce(
              [](bool* callback_done, mojom::LoadModelResult result) {
                EXPECT_EQ(result, mojom::LoadModelResult::OK);
                *callback_done = true;
              },
              &callback_done)
              .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(callback_done);
  ASSERT_TRUE(scanner.is_bound());

  constexpr int kNv12ImageSize = 256 * 256;
  std::vector<uint8_t> fake_nv12_data(kNv12ImageSize, 0);
  base::MappedReadOnlyRegion memory =
      base::ReadOnlySharedMemoryRegion::Create(fake_nv12_data.size());
  memcpy(memory.mapping.memory(), fake_nv12_data.data(), fake_nv12_data.size());

  mojom::DetectCornersResultPtr result = mojom::DetectCornersResult::New();
  result->status = mojom::DocumentScannerResultStatus::OK;
  result->corners = {};
  fake_service_connection.SetOutputDetectCornersResult(std::move(result));

  bool infer_callback_done = false;
  run_loop.reset(new base::RunLoop);
  scanner->DetectCornersFromNV12Image(
      std::move(memory.region),
      base::BindOnce(
          [](bool* infer_callback_done, mojom::DetectCornersResultPtr result) {
            *infer_callback_done = true;
            ASSERT_EQ(result->status, mojom::DocumentScannerResultStatus::OK);
            ASSERT_TRUE(result->corners.size() == 0);
          },
          &infer_callback_done)
          .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(infer_callback_done);
}

// Tests the fake ML service for web platform model loader.
TEST_F(ServiceConnectionTest,
       FakeServiceConnectionForLoadingWebPlatformModelLoader) {
  FakeServiceConnectionImpl fake_service_connection;
  ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  ServiceConnection::GetInstance()->Initialize();

  // First, tries to create a model loader;
  mojo::Remote<ml::model_loader::mojom::ModelLoader> model_loader;
  fake_service_connection.SetCreateWebPlatformModelLoaderResult(
      ml::model_loader::mojom::CreateModelLoaderResult::kOk);

  auto create_loader_options =
      ml::model_loader::mojom::CreateModelLoaderOptions::New();
  bool create_loader_callback_done = false;
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .CreateWebPlatformModelLoader(
          model_loader.BindNewPipeAndPassReceiver(),
          std::move(create_loader_options),
          base::BindOnce(
              [](bool* callback_done,
                 ml::model_loader::mojom::CreateModelLoaderResult result) {
                EXPECT_EQ(
                    result,
                    ml::model_loader::mojom::CreateModelLoaderResult::kOk);
                *callback_done = true;
              },
              &create_loader_callback_done)
              .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(create_loader_callback_done);
  ASSERT_TRUE(model_loader.is_bound());

  // Second, loads a model from the model loader.
  mojo::Remote<ml::model_loader::mojom::Model> model;
  mojo_base::BigBuffer model_content;
  bool create_model_callback_done = false;
  run_loop.reset(new base::RunLoop);
  fake_service_connection.SetLoadWebPlatformModelResult(
      ml::model_loader::mojom::LoadModelResult::kOk);
  auto model_info = ml::model_loader::mojom::ModelInfo::New();
  model_info->input_tensor_info["input1"] =
      ml::model_loader::mojom::TensorInfo::New(
          123u, ml::model_loader::mojom::DataType::kInt32,
          std::vector<unsigned int>({1u, 2u, 3u}));
  model_info->input_tensor_info["input2"] =
      ml::model_loader::mojom::TensorInfo::New(
          456u, ml::model_loader::mojom::DataType::kFloat64,
          std::vector<unsigned int>({3u}));
  model_info->output_tensor_info["output1"] =
      ml::model_loader::mojom::TensorInfo::New(
          789u, ml::model_loader::mojom::DataType::kUint16,
          std::vector<unsigned int>({3u, 4u, 5u}));
  model_info->output_tensor_info["output2"] =
      ml::model_loader::mojom::TensorInfo::New(
          654u, ml::model_loader::mojom::DataType::kUint8,
          std::vector<unsigned int>({7u, 8u}));
  fake_service_connection.SetWebPlatformModelInfo(std::move(model_info));
  model_loader->Load(
      std::move(model_content),
      base::BindOnce(
          [](bool* callback_done,
             mojo::Remote<ml::model_loader::mojom::Model>* model,
             ml::model_loader::mojom::LoadModelResult result,
             mojo::PendingRemote<ml::model_loader::mojom::Model> remote,
             ml::model_loader::mojom::ModelInfoPtr model_info) {
            *callback_done = true;
            // Check if the suggestion is correct.
            ASSERT_EQ(model_info->input_tensor_info.size(), 2u);
            ASSERT_EQ(model_info->output_tensor_info.size(), 2u);
            const auto iter1 = model_info->input_tensor_info.find("input1");
            ASSERT_TRUE(iter1 != model_info->input_tensor_info.end());
            EXPECT_EQ(iter1->second->byte_size, 123u);
            EXPECT_EQ(iter1->second->data_type,
                      ml::model_loader::mojom::DataType::kInt32);
            EXPECT_EQ(iter1->second->dimensions,
                      std::vector<unsigned int>({1u, 2u, 3u}));
            const auto iter2 = model_info->input_tensor_info.find("input2");
            ASSERT_TRUE(iter2 != model_info->input_tensor_info.end());
            EXPECT_EQ(iter2->second->byte_size, 456u);
            EXPECT_EQ(iter2->second->data_type,
                      ml::model_loader::mojom::DataType::kFloat64);
            EXPECT_EQ(iter2->second->dimensions,
                      std::vector<unsigned int>({3u}));
            const auto iter3 = model_info->output_tensor_info.find("output1");
            ASSERT_TRUE(iter3 != model_info->output_tensor_info.end());
            EXPECT_EQ(iter3->second->byte_size, 789u);
            EXPECT_EQ(iter3->second->data_type,
                      ml::model_loader::mojom::DataType::kUint16);
            EXPECT_EQ(iter3->second->dimensions,
                      std::vector<unsigned int>({3u, 4u, 5u}));
            const auto iter4 = model_info->output_tensor_info.find("output2");
            ASSERT_TRUE(iter4 != model_info->output_tensor_info.end());
            EXPECT_EQ(iter4->second->byte_size, 654u);
            EXPECT_EQ(iter4->second->data_type,
                      ml::model_loader::mojom::DataType::kUint8);
            EXPECT_EQ(iter4->second->dimensions,
                      std::vector<unsigned int>({7u, 8u}));

            ASSERT_TRUE(remote.is_valid());
            model->Bind(std::move(remote));
          },
          &create_model_callback_done, &model)
          .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(create_model_callback_done);
  ASSERT_TRUE(model.is_bound());

  // At last, calls the `Compute` interface of `Model`.
  base::flat_map<std::string, std::vector<uint8_t>> input_tensors;
  input_tensors["input1"] = {1, 2, 3};
  base::flat_map<std::string, std::vector<uint8_t>> output_tensors;
  output_tensors["output1"] = {10, 20, 30};
  fake_service_connection.SetOutputWebPlatformModelCompute(
      std::move(output_tensors));
  bool compute_callback_done = false;
  run_loop.reset(new base::RunLoop);
  model->Compute(
      std::move(input_tensors),
      base::BindOnce(
          [](bool* callback_done, ml::model_loader::mojom::ComputeResult result,
             const absl::optional<base::flat_map<
                 std::string, std::vector<uint8_t>>>& output_tensors) {
            ASSERT_TRUE(output_tensors.has_value());
            ASSERT_EQ(output_tensors->size(), 1u);
            const auto iter = output_tensors->find("output1");
            ASSERT_TRUE(iter != output_tensors->end());
            EXPECT_EQ(iter->second, std::vector<uint8_t>({10, 20, 30}));

            *callback_done = true;
          },
          &compute_callback_done)
          .Then(run_loop->QuitClosure()));
  run_loop->Run();
  ASSERT_TRUE(compute_callback_done);
}

}  // namespace
}  // namespace machine_learning
}  // namespace chromeos
