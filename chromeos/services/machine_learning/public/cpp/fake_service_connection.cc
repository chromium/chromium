// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"

#include <utility>
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/optional.h"

namespace chromeos {
namespace machine_learning {

FakeServiceConnectionImpl::FakeServiceConnectionImpl()
    : output_tensor_(mojom::Tensor::New()),
      load_handwriting_model_result_(mojom::LoadHandwritingModelResult::OK),
      load_model_result_(mojom::LoadModelResult::OK),
      load_text_classifier_result_(mojom::LoadModelResult::OK),
      create_graph_executor_result_(mojom::CreateGraphExecutorResult::OK),
      execute_result_(mojom::ExecuteResult::OK),
      async_mode_(false) {}

FakeServiceConnectionImpl::~FakeServiceConnectionImpl() {}

void FakeServiceConnectionImpl::LoadBuiltinModel(
    mojom::BuiltinModelSpecPtr spec,
    mojo::PendingReceiver<mojom::Model> receiver,
    mojom::MachineLearningService::LoadBuiltinModelCallback callback) {
  ScheduleCall(base::BindOnce(
      &FakeServiceConnectionImpl::HandleLoadBuiltinModelCall,
      base::Unretained(this), std::move(receiver), std::move(callback)));
}

void FakeServiceConnectionImpl::LoadFlatBufferModel(
    mojom::FlatBufferModelSpecPtr spec,
    mojo::PendingReceiver<mojom::Model> receiver,
    mojom::MachineLearningService::LoadFlatBufferModelCallback callback) {
  ScheduleCall(base::BindOnce(
      &FakeServiceConnectionImpl::HandleLoadFlatBufferModelCall,
      base::Unretained(this), std::move(receiver), std::move(callback)));
}

void FakeServiceConnectionImpl::CreateGraphExecutor(
    mojo::PendingReceiver<mojom::GraphExecutor> receiver,
    mojom::Model::CreateGraphExecutorCallback callback) {
  ScheduleCall(base::BindOnce(
      &FakeServiceConnectionImpl::HandleCreateGraphExecutorCall,
      base::Unretained(this), std::move(receiver), std::move(callback)));
}

// Fake impl of CreateGrapHExecutorWithOptions just ignores `options`.
void FakeServiceConnectionImpl::CreateGraphExecutorWithOptions(
    mojom::GraphExecutorOptionsPtr options,
    mojo::PendingReceiver<mojom::GraphExecutor> receiver,
    mojom::Model::CreateGraphExecutorCallback callback) {
  CreateGraphExecutor(std::move(receiver), std::move(callback));
}

void FakeServiceConnectionImpl::LoadTextClassifier(
    mojo::PendingReceiver<mojom::TextClassifier> receiver,
    mojom::MachineLearningService::LoadTextClassifierCallback callback) {
  ScheduleCall(base::BindOnce(
      &FakeServiceConnectionImpl::HandleLoadTextClassifierCall,
      base::Unretained(this), std::move(receiver), std::move(callback)));
}

void FakeServiceConnectionImpl::LoadHandwritingModel(
    mojom::HandwritingRecognizerSpecPtr spec,
    mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
    mojom::MachineLearningService::LoadHandwritingModelCallback callback) {
  ScheduleCall(base::BindOnce(
      &FakeServiceConnectionImpl::HandleLoadHandwritingModel,
      base::Unretained(this), std::move(receiver), std::move(callback)));
}

void FakeServiceConnectionImpl::LoadHandwritingModelWithSpec(
    mojom::HandwritingRecognizerSpecPtr spec,
    mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
    mojom::MachineLearningService::LoadHandwritingModelWithSpecCallback
        callback) {
  ScheduleCall(base::BindOnce(
      &FakeServiceConnectionImpl::HandleLoadHandwritingModelWithSpec,
      base::Unretained(this), std::move(receiver), std::move(callback)));
}

void FakeServiceConnectionImpl::Execute(
    base::flat_map<std::string, mojom::TensorPtr> inputs,
    const std::vector<std::string>& output_names,
    mojom::GraphExecutor::ExecuteCallback callback) {
  ScheduleCall(base::BindOnce(&FakeServiceConnectionImpl::HandleExecuteCall,
                              base::Unretained(this), std::move(callback)));
}

void FakeServiceConnectionImpl::SetLoadModelFailure() {
  load_model_result_ = mojom::LoadModelResult::LOAD_MODEL_ERROR;
}

void FakeServiceConnectionImpl::SetCreateGraphExecutorFailure() {
  load_model_result_ = mojom::LoadModelResult::OK;
  create_graph_executor_result_ =
      mojom::CreateGraphExecutorResult::MODEL_INTERPRETATION_ERROR;
}

void FakeServiceConnectionImpl::SetExecuteFailure() {
  load_model_result_ = mojom::LoadModelResult::OK;
  create_graph_executor_result_ = mojom::CreateGraphExecutorResult::OK;
  execute_result_ = mojom::ExecuteResult::EXECUTION_ERROR;
}

void FakeServiceConnectionImpl::SetExecuteSuccess() {
  load_model_result_ = mojom::LoadModelResult::OK;
  create_graph_executor_result_ = mojom::CreateGraphExecutorResult::OK;
  execute_result_ = mojom::ExecuteResult::OK;
}

void FakeServiceConnectionImpl::SetTextClassifierSuccess() {
  load_text_classifier_result_ = mojom::LoadModelResult::OK;
}

void FakeServiceConnectionImpl::SetLoadTextClassifierFailure() {
  load_text_classifier_result_ = mojom::LoadModelResult::LOAD_MODEL_ERROR;
}

void FakeServiceConnectionImpl::SetOutputValue(
    const std::vector<int64_t>& shape,
    const std::vector<double>& value) {
  output_tensor_->shape = mojom::Int64List::New();
  output_tensor_->shape->value = shape;
  output_tensor_->data = mojom::ValueList::New();
  output_tensor_->data->set_float_list(mojom::FloatList::New());
  output_tensor_->data->get_float_list()->value = value;
}

void FakeServiceConnectionImpl::SetAsyncMode(bool async_mode) {
  async_mode_ = async_mode;
}

void FakeServiceConnectionImpl::RunPendingCalls() {
  for (auto& call : pending_calls_) {
    std::move(call).Run();
  }

  pending_calls_.clear();
}

void FakeServiceConnectionImpl::HandleLoadBuiltinModelCall(
    mojo::PendingReceiver<mojom::Model> receiver,
    mojom::MachineLearningService::LoadBuiltinModelCallback callback) {
  if (load_model_result_ == mojom::LoadModelResult::OK)
    model_receivers_.Add(this, std::move(receiver));

  std::move(callback).Run(load_model_result_);
}

void FakeServiceConnectionImpl::HandleLoadTextClassifierCall(
    mojo::PendingReceiver<mojom::TextClassifier> receiver,
    mojom::MachineLearningService::LoadTextClassifierCallback callback) {
  if (load_text_classifier_result_ == mojom::LoadModelResult::OK)
    text_classifier_receivers_.Add(this, std::move(receiver));

  std::move(callback).Run(load_text_classifier_result_);
}

void FakeServiceConnectionImpl::ScheduleCall(base::OnceClosure call) {
  if (async_mode_)
    pending_calls_.push_back(std::move(call));
  else
    std::move(call).Run();
}

void FakeServiceConnectionImpl::HandleLoadFlatBufferModelCall(
    mojo::PendingReceiver<mojom::Model> receiver,
    mojom::MachineLearningService::LoadFlatBufferModelCallback callback) {
  if (load_model_result_ == mojom::LoadModelResult::OK)
    model_receivers_.Add(this, std::move(receiver));

  std::move(callback).Run(load_model_result_);
}

void FakeServiceConnectionImpl::HandleCreateGraphExecutorCall(
    mojo::PendingReceiver<mojom::GraphExecutor> receiver,
    mojom::Model::CreateGraphExecutorCallback callback) {
  if (create_graph_executor_result_ == mojom::CreateGraphExecutorResult::OK)
    graph_receivers_.Add(this, std::move(receiver));

  std::move(callback).Run(create_graph_executor_result_);
}

void FakeServiceConnectionImpl::HandleExecuteCall(
    mojom::GraphExecutor::ExecuteCallback callback) {
  if (execute_result_ != mojom::ExecuteResult::OK) {
    std::move(callback).Run(execute_result_, base::nullopt);
    return;
  }

  std::vector<mojom::TensorPtr> output_tensors;
  output_tensors.push_back(output_tensor_.Clone());
  std::move(callback).Run(execute_result_, std::move(output_tensors));
}

void FakeServiceConnectionImpl::HandleAnnotateCall(
    mojom::TextAnnotationRequestPtr request,
    mojom::TextClassifier::AnnotateCallback callback) {
  std::vector<mojom::TextAnnotationPtr> annotations;
  for (auto const& annotate : annotate_result_) {
    annotations.emplace_back(annotate.Clone());
  }
  std::move(callback).Run(std::move(annotations));
}

void FakeServiceConnectionImpl::HandleSuggestSelectionCall(
    mojom::TextSuggestSelectionRequestPtr request,
    mojom::TextClassifier::SuggestSelectionCallback callback) {
  auto selection = suggest_selection_result_.Clone();
  std::move(callback).Run(std::move(selection));
}

void FakeServiceConnectionImpl::HandleFindLanguagesCall(
    std::string request,
    mojom::TextClassifier::FindLanguagesCallback callback) {
  std::vector<mojom::TextLanguagePtr> languages;
  for (auto const& language : find_languages_result_) {
    languages.emplace_back(language.Clone());
  }
  std::move(callback).Run(std::move(languages));
}

void FakeServiceConnectionImpl::SetOutputAnnotation(
    const std::vector<mojom::TextAnnotationPtr>& annotations) {
  annotate_result_.clear();
  for (auto const& annotate : annotations) {
    annotate_result_.emplace_back(annotate.Clone());
  }
}

void FakeServiceConnectionImpl::SetOutputSelection(
    const mojom::CodepointSpanPtr& selection) {
  suggest_selection_result_ = selection.Clone();
}

void FakeServiceConnectionImpl::SetOutputLanguages(
    const std::vector<mojom::TextLanguagePtr>& languages) {
  find_languages_result_.clear();
  for (auto const& language : languages) {
    find_languages_result_.emplace_back(language.Clone());
  }
}

void FakeServiceConnectionImpl::SetOutputHandwritingRecognizerResult(
    const mojom::HandwritingRecognizerResultPtr& result) {
  handwriting_result_ = result.Clone();
}

void FakeServiceConnectionImpl::Annotate(
    mojom::TextAnnotationRequestPtr request,
    mojom::TextClassifier::AnnotateCallback callback) {
    ScheduleCall(base::BindOnce(
      &FakeServiceConnectionImpl::HandleAnnotateCall,
      base::Unretained(this), std::move(request), std::move(callback)));
}

void FakeServiceConnectionImpl::SuggestSelection(
    mojom::TextSuggestSelectionRequestPtr request,
    mojom::TextClassifier::SuggestSelectionCallback callback) {
  ScheduleCall(base::BindOnce(
      &FakeServiceConnectionImpl::HandleSuggestSelectionCall,
      base::Unretained(this), std::move(request), std::move(callback)));
}

void FakeServiceConnectionImpl::FindLanguages(
    const std::string& text,
    mojom::TextClassifier::FindLanguagesCallback callback) {
  ScheduleCall(base::BindOnce(
      &FakeServiceConnectionImpl::HandleFindLanguagesCall,
      base::Unretained(this), text, std::move(callback)));
}

void FakeServiceConnectionImpl::Recognize(
    mojom::HandwritingRecognitionQueryPtr query,
    mojom::HandwritingRecognizer::RecognizeCallback callback) {
  ScheduleCall(base::BindOnce(&FakeServiceConnectionImpl::HandleRecognize,
                              base::Unretained(this), std::move(query),
                              std::move(callback)));
}

void FakeServiceConnectionImpl::HandleLoadHandwritingModel(
    mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
    mojom::MachineLearningService::LoadHandwritingModelCallback callback) {
  if (load_handwriting_model_result_ == mojom::LoadHandwritingModelResult::OK)
    handwriting_receivers_.Add(this, std::move(receiver));

  std::move(callback).Run(load_handwriting_model_result_);
}

void FakeServiceConnectionImpl::HandleLoadHandwritingModelWithSpec(
    mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
    mojom::MachineLearningService::LoadHandwritingModelWithSpecCallback
        callback) {
  if (load_model_result_ == mojom::LoadModelResult::OK)
    handwriting_receivers_.Add(this, std::move(receiver));

  std::move(callback).Run(load_model_result_);
}

void FakeServiceConnectionImpl::HandleRecognize(
    mojom::HandwritingRecognitionQueryPtr query,
    mojom::HandwritingRecognizer::RecognizeCallback callback) {
  std::move(callback).Run(handwriting_result_.Clone());
}

}  // namespace machine_learning
}  // namespace chromeos
