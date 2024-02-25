// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_PLATFORM_OPTIONS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_PLATFORM_OPTIONS_H_

namespace segmentation_platform {

struct PlatformOptions {
  explicit PlatformOptions(bool force_refresh_results,
                           bool disable_model_execution_delay = false);

  ~PlatformOptions() = default;

  PlatformOptions(const PlatformOptions& other) = default;

  static PlatformOptions CreateDefault();

  // The segmentation platform will ignore all the valid results from previous
  // model executions, and re-run all the models and recompute segment
  // selections. Used for testing the model execution locally.
  bool force_refresh_results{false};

  // Models executing at startup will be executed after a delay. This flag is to
  // disable the delay and immediately run the model at startup with the delay.
  bool disable_model_execution_delay{false};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_PLATFORM_OPTIONS_H_
