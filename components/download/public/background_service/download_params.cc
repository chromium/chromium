// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/download_params.h"

#include "components/download/public/background_service/clients.h"

namespace download {

SchedulingParams::SchedulingParams()
    : cancel_time(base::Time::Max()),
      priority(Priority::DEFAULT),
      network_requirements(NetworkRequirements::NONE),
      battery_requirements(BatteryRequirements::BATTERY_INSENSITIVE) {}

bool SchedulingParams::operator==(const SchedulingParams& rhs) const {
  return network_requirements == rhs.network_requirements &&
         battery_requirements == rhs.battery_requirements &&
         priority == rhs.priority && cancel_time == rhs.cancel_time;
}

RequestParams::RequestParams()
    : method("GET"), fetch_error_body(false), require_safety_checks(true) {}

RequestParams::RequestParams(const RequestParams& other) = default;

DownloadParams::DownloadParams() : client(DownloadClient::INVALID) {}

DownloadParams::DownloadParams(const DownloadParams& other) = default;

DownloadParams::~DownloadParams() = default;

}  // namespace download
