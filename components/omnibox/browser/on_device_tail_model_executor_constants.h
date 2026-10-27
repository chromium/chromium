// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_EXECUTOR_CONSTANTS_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_EXECUTOR_CONSTANTS_H_

#include <cstddef>

// Constants for `OnDeviceTailModelExecutor`.
// This header is for internal use of this folder (`components/omnibox/browser`)
// only, exposed for testing purposes, and should not be used by client
// libraries.

// Default maximum number of RNN steps used by `OnDeviceTailModelExecutor` when
// not specified in the model metadata.
inline constexpr size_t kDefaultMaxNumSteps = 20;

// Default minimum probability threshold used by `OnDeviceTailModelExecutor`
// when not specified in the model metadata.
inline constexpr float kDefaultProbabilityThreshold = 0.01;

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_EXECUTOR_CONSTANTS_H_
