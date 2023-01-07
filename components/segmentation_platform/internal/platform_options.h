// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_PLATFORM_OPTIONS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_PLATFORM_OPTIONS_H_

namespace segmentation_platform {

struct PlatformOptions {
  explicit PlatformOptions(bool force_refresh_results);
  ~PlatformOptions() = default;

  PlatformOptions(const PlatformOptions& other) = default;

  static PlatformOptions CreateDefault();

  // The segmentation platform will ignore all the valid results from previous
  // model executions, and re-run all the models and recompute segment
  // selections. Used for testing the model execution locally.
  bool force_refresh_results{false};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_PLATFORM_OPTIONS_H_
