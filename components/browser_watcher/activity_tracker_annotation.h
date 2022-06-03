// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_ACTIVITY_TRACKER_ANNOTATION_H_
#define COMPONENTS_BROWSER_WATCHER_ACTIVITY_TRACKER_ANNOTATION_H_

#include <stdint.h>

#include "third_party/crashpad/crashpad/client/annotation.h"

namespace browser_watcher {

// A Crashpad annotation to store the location and size of the buffer used
// for activity tracking. This is used to retrieve and record tracked activities
// from the handler at crash time.
class ActivityTrackerAnnotation : public crashpad::Annotation {
 public:
  struct ValueType {
    uint64_t address;
    uint64_t size;
  };
  static constexpr Type kAnnotationType = Annotation::UserDefinedType(0xBAB);
  static const char kAnnotationName[];

  void SetValue(const void* address, size_t size);

  // Returns the sole instance of this class.
  static ActivityTrackerAnnotation* GetInstance();

 private:
  ActivityTrackerAnnotation();

  ValueType value_;
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_ACTIVITY_TRACKER_ANNOTATION_H_
