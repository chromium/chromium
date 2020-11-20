// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/in_process_tflite_predictor.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace machine_learning {

class InProcessTFLitePredictorTest : public ::testing::Test {
 public:
  const int32_t kTFLiteNumThreads = 4;
  const int32_t kInputTensorNum = 1;
  const int32_t kOutputTensorNum = 1;

  const int32_t kInputTensorDims = 4;
  const int32_t kOutputTensorDims = 2;

  const int32_t kInputTensorDim0 = 1;
  const int32_t kInputTensorDim1 = 32;
  const int32_t kInputTensorDim2 = 32;
  const int32_t kInputTensorDim3 = 3;

  const int32_t kOutputTensorDim0 = 1;
  const int32_t kOutputTensorDim1 = 10;

  InProcessTFLitePredictorTest() = default;
  ~InProcessTFLitePredictorTest() override = default;

  // Returns TFLite test model path
  std::string GetTFLiteTestPath() {
    // Location of generated test data (<(PROGRAM_DIR)/test_data).
    base::FilePath g_gen_test_data_directory;

    base::PathService::Get(chrome::DIR_GEN_TEST_DATA,
                           &g_gen_test_data_directory);
    g_gen_test_data_directory =
        g_gen_test_data_directory.Append("simple_test.tflite");

    return g_gen_test_data_directory.value().c_str();
  }
};

TEST_F(InProcessTFLitePredictorTest, TFLiteInitializationTest) {
  // Initialize the model
  std::string model_path = GetTFLiteTestPath();
  InProcessTFLitePredictor predictor(model_path, kTFLiteNumThreads);
  TfLiteStatus status = predictor.Initialize();
  EXPECT_EQ(status, kTfLiteOk);
}

TEST_F(InProcessTFLitePredictorTest, TFLiteTensorsCountTest) {
  // Initialize the model
  std::string model_path = GetTFLiteTestPath();
  InProcessTFLitePredictor predictor(model_path, kTFLiteNumThreads);
  TfLiteStatus status = predictor.Initialize();
  EXPECT_EQ(status, kTfLiteOk);

  EXPECT_EQ(predictor.GetInputTensorCount(), kInputTensorNum);
  EXPECT_EQ(predictor.GetOutputTensorCount(), kOutputTensorNum);
}

TEST_F(InProcessTFLitePredictorTest, TFLiteTensorsTest) {
  // Initialize the model
  std::string model_path = GetTFLiteTestPath();
  InProcessTFLitePredictor predictor(model_path, kTFLiteNumThreads);
  TfLiteStatus status = predictor.Initialize();
  EXPECT_EQ(status, kTfLiteOk);

  EXPECT_EQ(predictor.GetInputTensorNumDims(0), kInputTensorDims);
  EXPECT_EQ(predictor.GetOutputTensorNumDims(0), kOutputTensorDims);

  EXPECT_EQ(predictor.GetInputTensorDim(0, 0), kInputTensorDim0);
  EXPECT_EQ(predictor.GetInputTensorDim(0, 1), kInputTensorDim1);
  EXPECT_EQ(predictor.GetInputTensorDim(0, 2), kInputTensorDim2);
  EXPECT_EQ(predictor.GetInputTensorDim(0, 3), kInputTensorDim3);

  EXPECT_EQ(predictor.GetOutputTensorDim(0, 0), kOutputTensorDim0);
  EXPECT_EQ(predictor.GetOutputTensorDim(0, 1), kOutputTensorDim1);
}

TEST_F(InProcessTFLitePredictorTest, TFLiteEvaluationTest) {
  int const kOutputSize = 10;
  float expectedOutput[kOutputSize] = {
      -0.4936581, -0.32497078, -0.1705023, -0.38193324, 0.36136785,
      0.2177353,  0.32200375,  0.28686714, -0.21846706, -0.4200018};

  // Initialize the model
  std::string model_path = GetTFLiteTestPath();
  InProcessTFLitePredictor predictor(model_path, kTFLiteNumThreads);
  predictor.Initialize();

  // Initialize model input tensor
  TfLiteTensor* inputTensor = predictor.GetInputTensor(0);
  EXPECT_TRUE(inputTensor);

  int32_t tensorTotalDims = 1;
  for (int i = 0; i < predictor.GetInputTensorNumDims(0); i++)
    tensorTotalDims = tensorTotalDims * predictor.GetInputTensorDim(0, i);

  // Assign the input data to be constants.
  float* tensorData = static_cast<float*>(predictor.GetInputTensorData(0));
  for (int i = 0; i < tensorTotalDims; i++)
    tensorData[i] = 1.0;

  // Evaluate model and check output
  TfLiteStatus status = predictor.Evaluate();
  EXPECT_EQ(status, kTfLiteOk);
  float* outputData = (float*)predictor.GetOutputTensorData(0);
  int output_tensor_dim = predictor.GetOutputTensorDim(0, 1);
  EXPECT_EQ(output_tensor_dim, kOutputSize);
  for (int i = 0; i < output_tensor_dim; i++)
    EXPECT_NEAR(expectedOutput[i], outputData[i], 1e-5);
}

TEST_F(InProcessTFLitePredictorTest, TFLiteInterpreterThreadsSet) {
  // Initialize the model
  std::string model_path = GetTFLiteTestPath();
  InProcessTFLitePredictor predictor(model_path, kTFLiteNumThreads);
  EXPECT_EQ(kTFLiteNumThreads, predictor.GetTFLiteNumThreads());
}

}  // namespace machine_learning
