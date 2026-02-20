// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_H_

#include "base/files/file_path.h"
#include "base/threading/sequence_bound.h"
#include "components/keyed_service/core/keyed_service.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorDatabase;

class AccessibilityAnnotatorBackend : public KeyedService {
 public:
  AccessibilityAnnotatorBackend();

  AccessibilityAnnotatorBackend(const AccessibilityAnnotatorBackend&) = delete;
  AccessibilityAnnotatorBackend& operator=(
      const AccessibilityAnnotatorBackend&) = delete;

  // Initializes the database at the given path. Must be called before any other
  // methods.
  void Init(const base::FilePath& db_path);

 protected:
  ~AccessibilityAnnotatorBackend() override;

 private:
  base::SequenceBound<AccessibilityAnnotatorDatabase> db_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_BACKEND_H_
