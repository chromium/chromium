// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_TEST_ACCESSIBILITY_ANNOTATOR_BACKEND_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_TEST_ACCESSIBILITY_ANNOTATOR_BACKEND_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/sync/protocol/accessibility_annotation_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace accessibility_annotator {

class MockAccessibilityAnnotatorBackendObserver
    : public AccessibilityAnnotatorBackend::Observer {
 public:
  MockAccessibilityAnnotatorBackendObserver();
  ~MockAccessibilityAnnotatorBackendObserver() override;

  MOCK_METHOD(void,
              OnContentAnnotationsAdded,
              (history::VisitID,
               const AccessibilityAnnotatorBackend::ContentAnnotationsData&),
              (override));
  MOCK_METHOD(void,
              OnContentAnnotationsDeleted,
              (base::span<const history::VisitID>),
              (override));
  MOCK_METHOD(void, OnContentAnnotationsCleared, (), (override));
};

class TestAccessibilityAnnotatorBackend : public AccessibilityAnnotatorBackend {
 public:
  TestAccessibilityAnnotatorBackend();
  ~TestAccessibilityAnnotatorBackend() override;

  MOCK_METHOD(void, Init, (), (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetAccessibilityAnnotationControllerDelegate,
              (),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(base::optional_ref<const ContentAnnotationsData>,
              GetContentAnnotationsCacheData,
              (history::VisitID),
              (const, override));
  MOCK_METHOD(void,
              SetContentAnnotationsCacheData,
              (history::VisitID, ContentAnnotationsData),
              (override));
  MOCK_METHOD(void,
              RemoveContentAnnotationsCacheData,
              (base::span<const history::VisitID>),
              (override));
  MOCK_METHOD(void, ClearContentAnnotationsCache, (), (override));
  MOCK_METHOD(base::Value, GetDebugUICacheData, (), (const, override));
  MOCK_METHOD(void,
              GetSyncAnnotationsByTypes,
              (EntityTypeEnumSet,
               base::OnceCallback<void(
                   std::vector<sync_pb::AccessibilityAnnotationSpecifics>)>),
              (override));
  MOCK_METHOD(AccessibilityAnnotationSyncBridge*,
              accessibility_annotation_sync_bridge,
              (),
              (override));

  void SetSyncAnnotations(
      std::vector<sync_pb::AccessibilityAnnotationSpecifics> annotations);
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_TEST_ACCESSIBILITY_ANNOTATOR_BACKEND_H_
