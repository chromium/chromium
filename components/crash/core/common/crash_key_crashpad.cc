// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: This file is only compiled when Crashpad is used as the crash
// reporter.

#include "components/crash/core/common/crash_key.h"

#include "base/debug/crash_logging.h"
#include "components/crash/core/common/crash_key_base_support.h"
#include "third_party/crashpad/crashpad/client/annotation_list.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"

namespace crash_reporter {

void InitializeCrashKeys() {
  crashpad::AnnotationList::Register();
  InitializeCrashKeyBaseSupport();
}

// Returns a value for the crash key named |key_name|. For Crashpad-based
// clients, this returns the first instance found of the name.
std::string GetCrashKeyValue(const std::string& key_name) {
  auto* annotation_list = crashpad::AnnotationList::Get();
  if (annotation_list) {
    for (crashpad::Annotation* annotation : *annotation_list) {
      if (key_name == annotation->name()) {
        return std::string(static_cast<const char*>(annotation->value()),
                           annotation->size());
      }
    }
  }

  return std::string();
}

void InitializeCrashKeysForTesting() {
  InitializeCrashKeys();
}

void ResetCrashKeysForTesting() {
  // The AnnotationList should not be deleted because the static Annotation
  // object data still reference the link nodes.
  auto* annotation_list = crashpad::AnnotationList::Get();
  if (annotation_list) {
    for (crashpad::Annotation* annotation : *annotation_list) {
      annotation->Clear();
    }
  }

  base::debug::SetCrashKeyImplementation(nullptr);
}

}  // namespace crash_reporter
