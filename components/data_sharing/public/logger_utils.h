// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_LOGGER_UTILS_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_LOGGER_UTILS_H_

#include "base/logging.h"

// Helper macro to make logging easier and expose file metadata about the log
// source.
#define DATA_SHARING_LOG(log_source, logger, message)                          \
  do {                                                                         \
    if (logger && logger->ShouldEnableDebugLogs()) [[unlikely]] {              \
      logger->Log(base::Time::Now(), log_source, __FILE__, __LINE__, message); \
    }                                                                          \
  } while (0)

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_LOGGER_UTILS_H_
