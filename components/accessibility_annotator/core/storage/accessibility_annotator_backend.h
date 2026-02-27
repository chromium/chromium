// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/data_type_store.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace accessibility_annotator {

class AccessibilityAnnotationSyncBridge;
class AccessibilityAnnotatorDatabase;

class AccessibilityAnnotatorBackend : public KeyedService {
 public:
  AccessibilityAnnotatorBackend(
      version_info::Channel channel,
      syncer::RepeatingDataTypeStoreFactory data_type_store_factory);

  ~AccessibilityAnnotatorBackend() override;

  AccessibilityAnnotatorBackend(const AccessibilityAnnotatorBackend&) = delete;
  AccessibilityAnnotatorBackend& operator=(
      const AccessibilityAnnotatorBackend&) = delete;

  // Initializes the database at the given path. Must be called before any other
  // methods.
  void Init(const base::FilePath& db_path);

  // Returns DataTypeControllerDelegate for the accessibility annotation
  // datatype.
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetAccessibilityAnnotationControllerDelegate();

 private:
  base::SequenceBound<AccessibilityAnnotatorDatabase> db_;
  std::unique_ptr<AccessibilityAnnotationSyncBridge>
      accessibility_annotation_sync_bridge_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_H_
