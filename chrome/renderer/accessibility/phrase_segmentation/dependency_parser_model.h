// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_MODEL_H_

#include "base/files/file.h"
#include "third_party/tflite/src/tensorflow/lite/core/interpreter.h"

namespace tflite::task::core {
class TfLiteEngine;
}  // namespace tflite::task::core

// The state of the dependency parser model file.
// LINT.IfChange(DependencyParserModelState)
enum class DependencyParserModelState {
  // The dependency parser model state is not known.
  kUnknown,
  // The provided model file was not valid.
  kModelFileInvalid,
  // The dependency parser model's `base::File` is valid.
  kModelFileValid,
  // The dependency parser model is available for use with TFLite.
  kModelAvailable,

  // New values above this line.
  kMaxValue = kModelAvailable,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:DependencyParserModelState)

// This class handles the initialization and execution of a TFLite model for
// dependency parsing. The model is constructed using a model file loaded from
// the memory. The model predicts the dependency head for each word in a given
// sentence.
// Each instance of this should only be used from a single thread.
class DependencyParserModel {
 public:
  DependencyParserModel();
  ~DependencyParserModel();

  DependencyParserModel(const DependencyParserModel&) = delete;
  DependencyParserModel& operator=(const DependencyParserModel&) = delete;

  // Updates the dependency parser model for use by memory-mapping
  // the 'model_file' used for dependency parsing.
  void UpdateWithFile(base::File model_file);

  // Returns whether 'this' is initialized and is available to handle requests
  // to get the dependency head of each word in a sentence.
  bool IsAvailable() const;

  int64_t GetModelVersion() const;

  // Runs the TFLite dependency parser model on a string. This will return
  // a vector of dependency head for each word in the string.
  std::vector<unsigned int> GetDependencyHeads(std::vector<std::string> input);

 private:
  // Returns the dependency head of each node in a dependency graph. The input
  // is the the matrix of probabilities where input[i][j] signals the
  // probability that node i is the dependency head of node j. The i-th element
  // in the returned array signals the dependency head of the node.
  std::vector<unsigned int> SolveDependencies(
      base::span<const std::vector<float>> input);

  // The tflite model for dependency parsing.
  std::unique_ptr<tflite::task::core::TfLiteEngine> dependency_parser_model_;

  // The number of threads to use for model inference. -1 tells TFLite to use
  // its internal default logic.
  const int num_threads_ = -1;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_MODEL_H_
