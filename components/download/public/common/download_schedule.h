// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SCHEDULE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SCHEDULE_H_

#include "base/time/time.h"
#include "components/download/public/common/download_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace download {

// Contains all information to schedule a download, used by download later
// feature.
class COMPONENTS_DOWNLOAD_EXPORT DownloadSchedule {
 public:
  DownloadSchedule(bool only_on_wifi, absl::optional<base::Time> start_time);
  DownloadSchedule(const DownloadSchedule&);
  ~DownloadSchedule();

  bool operator==(const DownloadSchedule&) const;

  bool only_on_wifi() const { return only_on_wifi_; }

  const absl::optional<base::Time>& start_time() const { return start_time_; }

 private:
  // Whether to download only on WIFI. If true, |start_time_| will be ignored.
  bool only_on_wifi_;

  // Time to start the download. Will be ignored if |only_on_wifi_| is true.
  absl::optional<base::Time> start_time_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SCHEDULE_H_
