// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/onep_resolver_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"

namespace accessibility_annotator {

OnePResolverImpl::OnePResolverImpl() = default;

OnePResolverImpl::~OnePResolverImpl() = default;

void OnePResolverImpl::RetrieveAll(const std::u16string& query,
                                   RetrieveCallback callback) {
  if (!base::FeatureList::IsEnabled(
          kAccessibilityAnnotationReducerOnePResolver)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<MemorySearchResult>()));
    return;
  }

  // TODO(crbug.com/487416734): Implement real OneP network request.
  // For now, stub implementation:
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::vector<MemorySearchResult>()));
}

}  // namespace accessibility_annotator
