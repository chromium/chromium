// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_schedule.h"

#include "base/check.h"

namespace download {

DownloadSchedule::DownloadSchedule(bool only_on_wifi,
                                   absl::optional<base::Time> start_time)
    : only_on_wifi_(only_on_wifi), start_time_(start_time) {}

DownloadSchedule::DownloadSchedule(const DownloadSchedule&) = default;

DownloadSchedule::~DownloadSchedule() = default;

bool DownloadSchedule::operator==(const DownloadSchedule& other) const {
  return only_on_wifi_ == other.only_on_wifi() &&
         start_time_ == other.start_time();
}

}  // namespace download
