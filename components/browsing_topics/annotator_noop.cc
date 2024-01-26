// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/annotator_noop.h"

namespace browsing_topics {

AnnotatorNoOp::AnnotatorNoOp() = default;
AnnotatorNoOp::~AnnotatorNoOp() = default;

void AnnotatorNoOp::BatchAnnotate(BatchAnnotationCallback callback,
                                  const std::vector<std::string>& inputs) {
  std::vector<Annotation> annotations;
  annotations.reserve(inputs.size());
  for (const std::string& input : inputs) {
    annotations.push_back(Annotation(input));
  }
  std::move(callback).Run(annotations);
}

void AnnotatorNoOp::NotifyWhenModelAvailable(base::OnceClosure callback) {}

std::optional<optimization_guide::ModelInfo>
AnnotatorNoOp::GetBrowsingTopicsModelInfo() const {
  return std::nullopt;
}

}  // namespace browsing_topics
