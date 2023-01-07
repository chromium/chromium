// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/fuchsia/queryable_data_host_fuchsia.h"

#include "base/values.h"
#include "chromecast/common/queryable_data.h"

namespace chromecast {

QueryableDataHostFuchsia::QueryableDataHostFuchsia() {}

QueryableDataHostFuchsia::~QueryableDataHostFuchsia() {}

void QueryableDataHostFuchsia::SendQueryableValue(const std::string& key,
                                                  const base::Value& value) {
  // TODO(b/169167391): async call to update queryable values when runner fidl
  // exists.
  LOG(ERROR) << "Not Implemented: SendQueryableValue" << key << " " << value;
}

}  // namespace chromecast
