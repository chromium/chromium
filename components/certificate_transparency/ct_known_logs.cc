// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/ct_known_logs.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <iterator>

#include "base/time/time.h"
#include "crypto/sha2.h"

namespace certificate_transparency {

namespace {
#include "components/certificate_transparency/data/log_list-inc.cc"
}  // namespace

base::Time GetLogListTimestamp() {
  return kLogListTimestamp;
}

std::vector<CTLogInfo> GetKnownLogs() {
  // Add all qualified logs.
  std::vector<CTLogInfo> logs(std::begin(kCTLogList), std::end(kCTLogList));

  // Add all disqualified logs. Callers are expected to filter verified SCTs
  // via IsLogDisqualified().
  for (const auto& disqualified_log : kDisqualifiedCTLogList) {
    logs.push_back(disqualified_log.log_info);
  }

  return logs;
}

std::vector<std::pair<std::string, base::Time>> GetDisqualifiedLogs() {
  std::vector<std::pair<std::string, base::Time>> result;
  for (const auto& log : kDisqualifiedCTLogList) {
    result.push_back(
        std::make_pair(std::string(log.log_id, crypto::kSHA256Length),
                       log.disqualification_date));
  }
  return result;
}

}  // namespace certificate_transparency
