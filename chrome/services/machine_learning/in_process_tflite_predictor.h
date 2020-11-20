// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MACHINE_LEARNING_IN_PROCESS_TFLITE_PREDICTOR_H_
#define CHROME_SERVICES_MACHINE_LEARNING_IN_PROCESS_TFLITE_PREDICTOR_H_

#include <functional>
#include <string>

#include "base/memory/ptr_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/services/machine_learning/machine_learning_tflite_buildflags.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "third_party/tflite/src/tensorflow/lite/c/common.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/model.h"
#endif

namespace machine_learning {

// TFLite predictor class around TFLite C API for TFLite model evaluation.
class InProcessTFLitePredictor {
 public:
  InProcessTFLitePredictor(std::string filename, int32_t num_threads);
  ~InProcessTFLitePredictor();

  // Loads model, build the TFLite interpreter and allocates tensors.
  TfLiteStatus Initialize();

  // Invokes TFLite interpreter.
  TfLiteStatus Evaluate();

  // Returns number of input tensors.
  int32_t GetInputTensorCount() const;

  // Returns number of output tensors.
  int32_t GetOutputTensorCount() const;

  // Returns input tensor with |index| value starting from 0.
  TfLiteTensor* GetInputTensor(int32_t index) const;

  // Returns output tensor with |index| value starting from 0.
  const TfLiteTensor* GetOutputTensor(int32_t index) const;

  // Returns |initialized_|.
  bool IsInitialized() const;

  // Returns number of dimensions of input tensor |tensor_index|.
  int32_t GetInputTensorNumDims(int32_t tensor_index) const;

  // Returns value of dimension |dim_index| of input tensor |tensor_index|.
  int32_t GetInputTensorDim(int32_t tensor_index, int32_t dim_index) const;

  // Returns data pointer to input tensor with index |tensor_index|.
  void* GetInputTensorData(int32_t tensor_index) const;

  // Returns number of dimensions of output tensor |tensor_index|.
  int32_t GetOutputTensorNumDims(int32_t tensor_index) const;

  // Returns value of dimension |dim_index| of output tensor |tensor_index|.
  int32_t GetOutputTensorDim(int32_t tensor_index, int32_t dim_index) const;

  // Returns data pointer to output tensor with index |tensor_index|.
  void* GetOutputTensorData(int32_t tensor_index) const;

  // Returns TFLite interpreter number of threads.
  int32_t GetTFLiteNumThreads() const;

 private:
  // Loads TFLite model.
  bool LoadModel();

  // Builds TFLite interpreter.
  bool BuildInterpreter();

  // Allocates tensor for the current loaded model.
  TfLiteStatus AllocateTensors();

  std::string model_file_name_;

  // Number of threads used by |interpreter_| for evaluating |model_|.
  int32_t num_threads_ = 1;
  std::unique_ptr<tflite::FlatBufferModel> model_;
  std::unique_ptr<tflite::Interpreter> interpreter_;

  // True if TFLite interpreter is initialized.
  bool initialized_ = false;
};

}  // namespace machine_learning

#endif  // CHROME_SERVICES_MACHINE_LEARNING_IN_PROCESS_TFLITE_PREDICTOR_H_
