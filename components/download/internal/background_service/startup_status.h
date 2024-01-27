// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_STARTUP_STATUS_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_STARTUP_STATUS_H_

#include <optional>

namespace download {

// Helper struct to track the initialization status of various Controller
// internal components.
struct StartupStatus {
  StartupStatus();

  StartupStatus(const StartupStatus&) = delete;
  StartupStatus& operator=(const StartupStatus&) = delete;

  ~StartupStatus();

  std::optional<bool> driver_ok;
  std::optional<bool> model_ok;
  std::optional<bool> file_monitor_ok;

  // Resets all startup state tracking.
  void Reset();

  // Whether or not all components have finished initialization.  Note that this
  // does not mean that all components were initialized successfully.
  bool Complete() const;

  // Whether or not all components have initialized successfully.  Should only
  // be called if Complete() is true.
  bool Ok() const;

  // Whether any of the components have failed to initialize.
  bool Failed() const;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_STARTUP_STATUS_H_
