// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_data_provider_impl.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace accessibility_annotator {

AccessibilityAnnotatorDataProviderImpl::
    AccessibilityAnnotatorDataProviderImpl() = default;
AccessibilityAnnotatorDataProviderImpl::
    ~AccessibilityAnnotatorDataProviderImpl() = default;

void AccessibilityAnnotatorDataProviderImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityAnnotatorDataProviderImpl::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AccessibilityAnnotatorDataProviderImpl::GetEntities(
    EntityTypeEnumSet types,
    base::OnceCallback<void(std::vector<Entity>)> callback) {
  // TODO(crbug.com/486769736) - Implement this to read from db.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::vector<Entity>()));
}

}  // namespace accessibility_annotator
