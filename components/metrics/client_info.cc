// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/client_info.h"

namespace metrics {

ClientInfo::ClientInfo() : installation_date(0), reporting_enabled_date(0) {}

ClientInfo::~ClientInfo() = default;

}  // namespace metrics
