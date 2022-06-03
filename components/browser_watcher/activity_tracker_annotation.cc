// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/activity_tracker_annotation.h"

namespace browser_watcher {

const char ActivityTrackerAnnotation::kAnnotationName[] =
    "ActivityTrackerLocation";

ActivityTrackerAnnotation::ActivityTrackerAnnotation()
    : crashpad::Annotation(kAnnotationType, kAnnotationName, &value_) {}

void ActivityTrackerAnnotation::SetValue(const void* address, size_t size) {
  value_.address = reinterpret_cast<uint64_t>(address);
  value_.size = size;
  SetSize(sizeof(value_));
}

// static
ActivityTrackerAnnotation* ActivityTrackerAnnotation::GetInstance() {
  // This object is intentionally leaked.
  static ActivityTrackerAnnotation* instance = new ActivityTrackerAnnotation();
  return instance;
}

}  // namespace browser_watcher
