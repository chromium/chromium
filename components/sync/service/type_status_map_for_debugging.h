// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_TYPE_STATUS_MAP_FOR_DEBUGGING_H_
#define COMPONENTS_SYNC_SERVICE_TYPE_STATUS_MAP_FOR_DEBUGGING_H_

#include <map>

#include "components/sync/base/data_type.h"

namespace syncer {

// Used to populate chrome://sync-internals' detailed type status table.
struct TypeStatusForDebugging {
  enum class Severity {
    kError,
    kWarning,
    kInfo,
    kTransitioning,
    kOk,
  };

  TypeStatusForDebugging() = default;
  ~TypeStatusForDebugging() = default;

  Severity severity = Severity::kOk;
  std::string state;
  std::string message;
};

using TypeStatusMapForDebugging = std::map<DataType, TypeStatusForDebugging>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_TYPE_STATUS_MAP_FOR_DEBUGGING_H_
