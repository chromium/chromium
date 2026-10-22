// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PROGRESS_MARKER_MAP_H_
#define COMPONENTS_SYNC_BASE_PROGRESS_MARKER_MAP_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"

namespace syncer {

// A container that maps DataType to serialized
// DataTypeProgressMarkers.
using ProgressMarkerMap = base::flat_map<DataType, std::string>;

base::DictValue ProgressMarkerMapToValueDict(
    const ProgressMarkerMap& marker_map);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_PROGRESS_MARKER_MAP_H_
