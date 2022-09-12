// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_ACTIVITY_QUERYABLE_DATA_HOST_H_
#define CHROMECAST_ACTIVITY_QUERYABLE_DATA_HOST_H_

#include <string>

#include "base/values.h"

namespace chromecast {

// Sends queryable data to a host where it will be consumed.
// Either through to WebContents (non-fuchsia) or through FIDL to the
// Cast Runner (Fuchsia).
class QueryableDataHost {
 public:
  virtual ~QueryableDataHost() = default;

  // Send the queryable data entry |key|:|value|
  virtual void SendQueryableValue(const std::string& key,
                                  const base::Value& value) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_ACTIVITY_QUERYABLE_DATA_HOST_H_
