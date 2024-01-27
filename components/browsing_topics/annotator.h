// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_ANNOTATOR_H_
#define COMPONENTS_BROWSING_TOPICS_ANNOTATOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/optimization_guide/core/bert_model_handler.h"

namespace browsing_topics {

// An annotation for a single input to the Annotator.
struct Annotation {
  explicit Annotation(const std::string& input);
  Annotation(const Annotation& other);
  ~Annotation();

  std::string input;
  std::vector<int32_t> topics;
};

// An ML-based annotator for using in Browsing Topics. Models are delivered by
// the OptimizationGuide service and processed using the TFLite Support Library.
// Support for a Google-provided lookup table is also included that
// short-circuits the ML model, allowing for better performance on popular pages
// as well as human labels when the ML model is inaccurate.
class Annotator {
 public:
  using BatchAnnotationCallback =
      base::OnceCallback<void(const std::vector<Annotation>&)>;

  Annotator() = default;
  virtual ~Annotator() = default;

  // This is the main entry point for callers.
  virtual void BatchAnnotate(BatchAnnotationCallback callback,
                             const std::vector<std::string>& inputs) = 0;

  // Runs |callback| when the model is ready to execute. If the model is ready
  // now, the callback is run immediately.
  virtual void NotifyWhenModelAvailable(base::OnceClosure callback) = 0;

  // Returns the model info if the model file is available.
  virtual std::optional<optimization_guide::ModelInfo>
  GetBrowsingTopicsModelInfo() const = 0;
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_ANNOTATOR_H_
