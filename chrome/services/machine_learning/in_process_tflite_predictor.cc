// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/in_process_tflite_predictor.h"

#include "base/check.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/register.h"
#include "third_party/tflite/src/tensorflow/lite/model.h"

namespace machine_learning {

InProcessTFLitePredictor::InProcessTFLitePredictor(std::string filename,
                                                   int32_t num_threads)
    : model_file_name_(filename), num_threads_(num_threads) {}

InProcessTFLitePredictor::~InProcessTFLitePredictor() = default;

TfLiteStatus InProcessTFLitePredictor::Initialize() {
  if (!LoadModel())
    return kTfLiteError;
  if (!BuildInterpreter())
    return kTfLiteError;
  TfLiteStatus status = AllocateTensors();
  if (status == kTfLiteOk)
    initialized_ = true;
  return status;
}

TfLiteStatus InProcessTFLitePredictor::Evaluate() {
  return interpreter_->Invoke();
}

bool InProcessTFLitePredictor::LoadModel() {
  if (model_file_name_.empty())
    return false;

  // We create the pointer using this approach since |TfLiteModel| is a
  // structure without the delete operator.
  model_ = tflite::FlatBufferModel::BuildFromFile(model_file_name_.c_str());
  if (model_ == nullptr)
    return false;

  return true;
}

bool InProcessTFLitePredictor::BuildInterpreter() {
  tflite::ops::builtin::BuiltinOpResolver resolver;
  tflite::InterpreterBuilder builder(*model_, resolver);

  if (builder(&interpreter_, num_threads_) != kTfLiteOk || !interpreter_)
    return false;

  return true;
}

TfLiteStatus InProcessTFLitePredictor::AllocateTensors() {
  TfLiteStatus status = interpreter_->AllocateTensors();
  DCHECK(status == kTfLiteOk);
  return status;
}

int32_t InProcessTFLitePredictor::GetInputTensorCount() const {
  if (interpreter_ == nullptr)
    return 0;
  return static_cast<int32_t>(interpreter_->inputs().size());
}

int32_t InProcessTFLitePredictor::GetOutputTensorCount() const {
  if (interpreter_ == nullptr)
    return 0;
  return static_cast<int32_t>(interpreter_->outputs().size());
}

TfLiteTensor* InProcessTFLitePredictor::GetInputTensor(int32_t index) const {
  if (interpreter_ == nullptr)
    return nullptr;
  return interpreter_->input_tensor(index);
}

const TfLiteTensor* InProcessTFLitePredictor::GetOutputTensor(
    int32_t index) const {
  if (interpreter_ == nullptr)
    return nullptr;
  return interpreter_->output_tensor(index);
}

bool InProcessTFLitePredictor::IsInitialized() const {
  return initialized_;
}

int32_t InProcessTFLitePredictor::GetInputTensorNumDims(
    int32_t tensor_index) const {
  TfLiteTensor* tensor = GetInputTensor(tensor_index);
  if (tensor)
    return tensor->dims->size;
  return 0;
}

int32_t InProcessTFLitePredictor::GetInputTensorDim(int32_t tensor_index,
                                                    int32_t dim_index) const {
  TfLiteTensor* tensor = GetInputTensor(tensor_index);
  if (tensor)
    return tensor->dims->data[dim_index];
  return 0;
}

void* InProcessTFLitePredictor::GetInputTensorData(int32_t tensor_index) const {
  TfLiteTensor* tensor = GetInputTensor(tensor_index);
  if (tensor)
    return static_cast<void*>(tensor->data.raw);
  return nullptr;
}

int32_t InProcessTFLitePredictor::GetOutputTensorNumDims(
    int32_t tensor_index) const {
  const TfLiteTensor* tensor = GetOutputTensor(tensor_index);
  if (tensor)
    return tensor->dims->size;
  return 0;
}

int32_t InProcessTFLitePredictor::GetOutputTensorDim(int32_t tensor_index,
                                                     int32_t dim_index) const {
  const TfLiteTensor* tensor = GetOutputTensor(tensor_index);
  if (tensor)
    return tensor->dims->data[dim_index];
  return 0;
}

void* InProcessTFLitePredictor::GetOutputTensorData(
    int32_t tensor_index) const {
  const TfLiteTensor* tensor = GetOutputTensor(tensor_index);
  if (tensor)
    return static_cast<void*>(tensor->data.raw);
  return nullptr;
}

int32_t InProcessTFLitePredictor::GetTFLiteNumThreads() const {
  return num_threads_;
}

}  // namespace machine_learning
