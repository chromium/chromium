// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/data_with_timestamp.h"

#include <sstream>

#include "base/check.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace ash::secure_channel {

DataWithTimestamp::DataWithTimestamp(const std::string& data,
                                     const int64_t start_timestamp_ms,
                                     const int64_t end_timestamp_ms)
    : data(data),
      start_timestamp_ms(start_timestamp_ms),
      end_timestamp_ms(end_timestamp_ms) {
  DCHECK(start_timestamp_ms < end_timestamp_ms);
  DCHECK(data.size());
}

DataWithTimestamp::DataWithTimestamp(const DataWithTimestamp& other)
    : data(other.data),
      start_timestamp_ms(other.start_timestamp_ms),
      end_timestamp_ms(other.end_timestamp_ms) {
  DCHECK(start_timestamp_ms < end_timestamp_ms);
  DCHECK(data.size());
}

// static.
std::string DataWithTimestamp::ToDebugString(
    const std::vector<DataWithTimestamp>& data_with_timestamps) {
  std::stringstream ss;
  ss << "[";
  for (const DataWithTimestamp& data : data_with_timestamps) {
    ss << "\n  (data: " << data.DataInHex() << ", start: "
       << base::UTF16ToUTF8(base::TimeFormatShortDateAndTimeWithTimeZone(
              base::Time::FromMillisecondsSinceUnixEpoch(
                  data.start_timestamp_ms)))
       << ", end: "
       << base::UTF16ToUTF8(base::TimeFormatShortDateAndTimeWithTimeZone(
              base::Time::FromMillisecondsSinceUnixEpoch(
                  data.end_timestamp_ms)))
       << "),";
  }
  ss << "\n]";
  return ss.str();
}

bool DataWithTimestamp::ContainsTime(const int64_t timestamp_ms) const {
  return start_timestamp_ms <= timestamp_ms && timestamp_ms < end_timestamp_ms;
}

std::string DataWithTimestamp::DataInHex() const {
  return "0x" + base::HexEncode(data);
}

bool DataWithTimestamp::operator==(const DataWithTimestamp& other) const {
  return data == other.data && start_timestamp_ms == other.start_timestamp_ms &&
         end_timestamp_ms == other.end_timestamp_ms;
}

}  // namespace ash::secure_channel
