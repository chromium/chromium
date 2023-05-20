// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/data_type_manager.h"

#include "base/notreached.h"

namespace syncer {

DataTypeManager::ConfigureResult::ConfigureResult() : status(UNKNOWN) {}

DataTypeManager::ConfigureResult::ConfigureResult(ConfigureStatus status,
                                                  ModelTypeSet requested_types)
    : status(status), requested_types(requested_types) {}

DataTypeManager::ConfigureResult::ConfigureResult(
    const ConfigureResult& other) = default;

DataTypeManager::ConfigureResult::~ConfigureResult() = default;

// Static.
std::string DataTypeManager::ConfigureStatusToString(ConfigureStatus status) {
  switch (status) {
    case OK:
      return "Ok";
    case ABORTED:
      return "Aborted";
    case UNKNOWN:
      NOTREACHED();
      return std::string();
  }
  return std::string();
}

}  // namespace syncer
