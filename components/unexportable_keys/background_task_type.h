// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_TYPE_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_TYPE_H_

#include <string_view>

#include "base/component_export.h"

namespace unexportable_keys {

// Enum containing all supported types of background TPM operations.
// These values are primarily used for histograms together with
// `GetBackgroundTaskTypeSuffixForHistograms()` below.
enum class BackgroundTaskType { kGenerateKey, kFromWrappedKey, kSign };

// Converts `BackgroundTaskType` to a histogram suffix string. The string is
// prepended with "." symbol so it can be directly concatenated with a base
// histogram name.
COMPONENT_EXPORT(UNEXPORTABLE_KEYS)
std::string_view GetBackgroundTaskTypeSuffixForHistograms(
    BackgroundTaskType type);

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_TYPE_H_
