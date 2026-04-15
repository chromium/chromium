// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/test_accessibility_annotator_backend.h"

#include <utility>

#include "base/functional/callback.h"
#include "components/accessibility_annotator/core/data_models/entity_converter.h"

namespace accessibility_annotator {

MockAccessibilityAnnotatorBackendObserver::
    MockAccessibilityAnnotatorBackendObserver() = default;

MockAccessibilityAnnotatorBackendObserver::
    ~MockAccessibilityAnnotatorBackendObserver() = default;

TestAccessibilityAnnotatorBackend::TestAccessibilityAnnotatorBackend() =
    default;

TestAccessibilityAnnotatorBackend::~TestAccessibilityAnnotatorBackend() =
    default;

void TestAccessibilityAnnotatorBackend::SetSyncAnnotations(
    std::vector<sync_pb::AccessibilityAnnotationSpecifics> annotations) {
  ON_CALL(*this, GetSyncAnnotationsByTypes(testing::_, testing::_))
      .WillByDefault(
          [annotations](
              EntityTypeEnumSet types,
              base::OnceCallback<void(
                  std::vector<sync_pb::AccessibilityAnnotationSpecifics>)>
                  callback) {
            std::vector<sync_pb::AccessibilityAnnotationSpecifics> results;
            for (const auto& specifics : annotations) {
              std::optional<EntityType> type =
                  GetEntityTypeFromSpecifics(specifics);
              if (type.has_value() && types.Has(*type)) {
                results.push_back(specifics);
              }
            }
            std::move(callback).Run(std::move(results));
          });
}

}  // namespace accessibility_annotator
