// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/background_task_priority.h"

#include <string_view>

#include "base/notreached.h"

namespace unexportable_keys {

std::string_view GetBackgroundTaskPrioritySuffixForHistograms(
    BackgroundTaskPriority priority) {
  switch (priority) {
    case BackgroundTaskPriority::kBestEffort:
      return ".BestEffort";
    case BackgroundTaskPriority::kUserVisible:
      return ".UserVisible";
    case BackgroundTaskPriority::kUserBlocking:
      return ".UserBlocking";
  }
  NOTREACHED();
}

}  // namespace unexportable_keys
