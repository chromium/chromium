// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/data_type_manager.h"

namespace syncer {

// Static.
std::string DataTypeManager::ConfigureStatusToString(ConfigureStatus status) {
  switch (status) {
    case OK:
      return "Ok";
    case ABORTED:
      return "Aborted";
  }
  return std::string();
}

}  // namespace syncer
