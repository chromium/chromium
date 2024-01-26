// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_ANNOTATOR_NOOP_H_
#define COMPONENTS_BROWSING_TOPICS_ANNOTATOR_NOOP_H_

#include "components/browsing_topics/annotator.h"

namespace browsing_topics {

// A no-op annotator when TFLite is not available by buildflag.
class AnnotatorNoOp : public Annotator {
 public:
  using BatchAnnotationCallback =
      base::OnceCallback<void(const std::vector<Annotation>&)>;

  AnnotatorNoOp();
  ~AnnotatorNoOp() override;

  // Annotator:
  void BatchAnnotate(BatchAnnotationCallback callback,
                     const std::vector<std::string>& inputs) override;
  void NotifyWhenModelAvailable(base::OnceClosure callback) override;
  std::optional<optimization_guide::ModelInfo> GetBrowsingTopicsModelInfo()
      const override;
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_ANNOTATOR_NOOP_H_
