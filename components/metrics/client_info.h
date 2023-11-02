// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CLIENT_INFO_H_
#define COMPONENTS_METRICS_CLIENT_INFO_H_

#include <stdint.h>

#include <string>

namespace metrics {

// A data object used to pass data from outside the metrics component into the
// metrics component.
struct ClientInfo {
 public:
  ClientInfo();
  ~ClientInfo();

  // The metrics ID of this client: represented as a GUID string.
  std::string client_id;

  // The installation date: represented as an epoch time in seconds.
  int64_t installation_date;

  // The date on which metrics reporting was enabled: represented as an epoch
  // time in seconds.
  int64_t reporting_enabled_date;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CLIENT_INFO_H_
