// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/background_task_type.h"

#include <string_view>

#include "base/notreached.h"

namespace unexportable_keys {

std::string_view GetBackgroundTaskTypeSuffixForHistograms(
    BackgroundTaskType type) {
  switch (type) {
    case BackgroundTaskType::kGenerateKey:
      return ".GenerateKey";
    case BackgroundTaskType::kFromWrappedKey:
      return ".FromWrappedKey";
    case BackgroundTaskType::kSign:
      return ".Sign";
  }
  NOTREACHED();
}

}  // namespace unexportable_keys
