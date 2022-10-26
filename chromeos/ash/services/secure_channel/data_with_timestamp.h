// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DATA_WITH_TIMESTAMP_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DATA_WITH_TIMESTAMP_H_

#include <string>
#include <vector>

namespace ash::secure_channel {

// Stores EID-related data and timestamps at which time this data becomes
// active or inactive.
struct DataWithTimestamp {
  DataWithTimestamp(const std::string& data,
                    const int64_t start_timestamp_ms,
                    const int64_t end_timestamp_ms);
  DataWithTimestamp(const DataWithTimestamp& other);

  // Helper function to convert a vector of DataWithTimestamp objects to a human
  // readable debug string.
  static std::string ToDebugString(
      const std::vector<DataWithTimestamp>& data_with_timestamps);

  bool ContainsTime(const int64_t timestamp_ms) const;
  std::string DataInHex() const;

  bool operator==(const DataWithTimestamp& other) const;

  // The data valid for a given time period.
  std::string data;

  // The start time for which the data is valid, inclusive.
  int64_t start_timestamp_ms;

  // The end time for which the data is valid, exclusive.
  int64_t end_timestamp_ms;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DATA_WITH_TIMESTAMP_H_
