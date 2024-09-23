// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/progress_marker_map.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/values.h"

namespace syncer {

base::Value::Dict ProgressMarkerMapToValueDict(
    const ProgressMarkerMap& marker_map) {
  base::Value::Dict value;
  for (const auto& [data_type, progress_marker] : marker_map) {
    std::string printable_payload;
    base::EscapeJSONString(progress_marker, false /* put_in_quotes */,
                           &printable_payload);
    printable_payload = base::Base64Encode(printable_payload);
    value.Set(DataTypeToDebugString(data_type), printable_payload);
  }
  return value;
}

}  // namespace syncer
