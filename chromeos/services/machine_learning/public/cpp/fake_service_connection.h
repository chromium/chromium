// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/handwriting_recognizer.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/tensor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/text_classifier.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace machine_learning {

// Fake implementation of chromeos::machine_learning::ServiceConnection.
// Handles LoadModel (and Model::CreateGraphExecutor) by binding to itself.
// Handles GraphExecutor::Execute by always returning the value specified by
// a previous call to SetOutputValue.
// Handles TextClassifier::Annotate by always returning the value specified by
// a previous call to SetOutputAnnotation.
// Handles TextClassifier::SuggestSelection by always returning the value
// specified by a previous call to SetOutputSelection.
// For use with ServiceConnection::UseFakeServiceConnectionForTesting().
class FakeServiceConnectionImpl : public ServiceConnection,
                                  public mojom::Model,
                                  public mojom::TextClassifier,
                                  public mojom::HandwritingRecognizer,
                                  public mojom::GraphExecutor {
 public:
  FakeServiceConnectionImpl();
  ~FakeServiceConnectionImpl() override;

  // It's safe to execute LoadBuiltinModel, LoadFlatBufferModel and
  // LoadTextClassifier for multi times, but all the receivers will be bound to
  // the same instance.
  void LoadBuiltinModel(mojom::BuiltinModelSpecPtr spec,
                        mojo::PendingReceiver<mojom::Model> receiver,
                        mojom::MachineLearningService::LoadBuiltinModelCallback
                            callback) override;
  void LoadFlatBufferModel(
      mojom::FlatBufferModelSpecPtr spec,
      mojo::PendingReceiver<mojom::Model> receiver,
      mojom::MachineLearningService::LoadFlatBufferModelCallback callback)
      override;

  void LoadTextClassifier(
      mojo::PendingReceiver<mojom::TextClassifier> receiver,
      mojom::MachineLearningService::LoadTextClassifierCallback callback)
      override;

  void LoadHandwritingModel(
      mojom::HandwritingRecognizerSpecPtr spec,
      mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
      mojom::MachineLearningService::LoadHandwritingModelCallback
          result_callback) override;

  // Will be deprecated and removed soon.
  void LoadHandwritingModelWithSpec(
      mojom::HandwritingRecognizerSpecPtr spec,
      mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
      mojom::MachineLearningService::LoadHandwritingModelWithSpecCallback
          result_callback) override;

  // mojom::Model:
  void CreateGraphExecutor(
      mojo::PendingReceiver<mojom::GraphExecutor> receiver,
      mojom::Model::CreateGraphExecutorCallback callback) override;
  void CreateGraphExecutorWithOptions(
      mojom::GraphExecutorOptionsPtr options,
      mojo::PendingReceiver<mojom::GraphExecutor> receiver,
      mojom::Model::CreateGraphExecutorCallback callback) override;

  // mojom::GraphExecutor:
  // Execute() will return the tensor set by SetOutputValue() as the output.
  void Execute(base::flat_map<std::string, mojom::TensorPtr> inputs,
               const std::vector<std::string>& output_names,
               mojom::GraphExecutor::ExecuteCallback callback) override;

  // Useful for simulating a failure at different stage.
  // There are different error codes at each stage, we just randomly pick one.
  void SetLoadModelFailure();
  void SetCreateGraphExecutorFailure();
  void SetExecuteFailure();
  void SetLoadTextClassifierFailure();
  // Reset all the Model related failures and make Execute succeed.
  void SetExecuteSuccess();
  // Reset all the TextClassifier related failures and make LoadTextClassifier
  // succeed.
  // Currently, there are three interfaces related to TextClassifier
  // (|LoadTextClassifier|, |Annotate| and |SuggestSelection|) but only
  // |LoadTextClassifier| can fail.
  void SetTextClassifierSuccess();

  // Call SetOutputValue() before Execute() to set the output tensor.
  void SetOutputValue(const std::vector<int64_t>& shape,
                      const std::vector<double>& value);

  // In async mode, FakeServiceConnectionImpl adds requests like
  // LoadBuiltinModel, CreateGraphExecutor to |pending_calls_| instead of
  // responding immediately. Calls in |pending_calls_| will run when
  // RunPendingCalls() is called.
  // It's useful when an unit test wants to test the async behaviour of real
  // ml-service.
  void SetAsyncMode(bool async_mode);
  void RunPendingCalls();

  // Call SetOutputAnnotation() before Annotate() to set the output annotation.
  void SetOutputAnnotation(
      const std::vector<mojom::TextAnnotationPtr>& annotation);

  // Call SetOutputSelection() before SuggestSelection() to set the output
  // selection.
  void SetOutputSelection(const mojom::CodepointSpanPtr& selection);

  // Call SetOutputLanguages() before FindLanguages() to set the output
  // languages.
  void SetOutputLanguages(const std::vector<mojom::TextLanguagePtr>& languages);

  // Call SetOutputHandwritingRecognizerResult() before Recognize() to set the
  // output of handwriting.
  void SetOutputHandwritingRecognizerResult(
      const mojom::HandwritingRecognizerResultPtr& result);

  // mojom::TextClassifier:
  void Annotate(mojom::TextAnnotationRequestPtr request,
                mojom::TextClassifier::AnnotateCallback callback) override;

  // mojom::TextClassifier:
  void SuggestSelection(
      mojom::TextSuggestSelectionRequestPtr request,
      mojom::TextClassifier::SuggestSelectionCallback callback) override;

  // mojom::TextClassifier:
  void FindLanguages(
      const std::string& text,
      mojom::TextClassifier::FindLanguagesCallback callback) override;

  // mojom::HandwritingRecognizer:
  void Recognize(
      mojom::HandwritingRecognitionQueryPtr query,
      mojom::HandwritingRecognizer::RecognizeCallback callback) override;

 private:
  void ScheduleCall(base::OnceClosure call);
  void HandleLoadBuiltinModelCall(
      mojo::PendingReceiver<mojom::Model> receiver,
      mojom::MachineLearningService::LoadBuiltinModelCallback callback);
  void HandleLoadFlatBufferModelCall(
      mojo::PendingReceiver<mojom::Model> receiver,
      mojom::MachineLearningService::LoadFlatBufferModelCallback callback);
  void HandleCreateGraphExecutorCall(
      mojo::PendingReceiver<mojom::GraphExecutor> receiver,
      mojom::Model::CreateGraphExecutorCallback callback);
  void HandleExecuteCall(mojom::GraphExecutor::ExecuteCallback callback);
  void HandleLoadTextClassifierCall(
      mojo::PendingReceiver<mojom::TextClassifier> receiver,
      mojom::MachineLearningService::LoadTextClassifierCallback callback);
  void HandleAnnotateCall(mojom::TextAnnotationRequestPtr request,
                          mojom::TextClassifier::AnnotateCallback callback);
  void HandleSuggestSelectionCall(
      mojom::TextSuggestSelectionRequestPtr request,
      mojom::TextClassifier::SuggestSelectionCallback callback);
  void HandleFindLanguagesCall(
      std::string text,
      mojom::TextClassifier::FindLanguagesCallback callback);
  void HandleLoadHandwritingModel(
      mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
      mojom::MachineLearningService::LoadHandwritingModelCallback callback);
  void HandleLoadHandwritingModelWithSpec(
      mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
      mojom::MachineLearningService::LoadHandwritingModelWithSpecCallback
          callback);
  void HandleRecognize(
      mojom::HandwritingRecognitionQueryPtr query,
      mojom::HandwritingRecognizer::RecognizeCallback callback);

  mojo::ReceiverSet<mojom::Model> model_receivers_;
  mojo::ReceiverSet<mojom::GraphExecutor> graph_receivers_;
  mojo::ReceiverSet<mojom::TextClassifier> text_classifier_receivers_;
  mojo::ReceiverSet<mojom::HandwritingRecognizer> handwriting_receivers_;
  mojom::TensorPtr output_tensor_;
  mojom::LoadHandwritingModelResult load_handwriting_model_result_;
  mojom::LoadModelResult load_model_result_;
  mojom::LoadModelResult load_text_classifier_result_;
  mojom::CreateGraphExecutorResult create_graph_executor_result_;
  mojom::ExecuteResult execute_result_;
  std::vector<mojom::TextAnnotationPtr> annotate_result_;
  mojom::CodepointSpanPtr suggest_selection_result_;
  std::vector<mojom::TextLanguagePtr> find_languages_result_;
  mojom::HandwritingRecognizerResultPtr handwriting_result_;

  bool async_mode_;
  std::vector<base::OnceClosure> pending_calls_;

  DISALLOW_COPY_AND_ASSIGN(FakeServiceConnectionImpl);
};

}  // namespace machine_learning
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
