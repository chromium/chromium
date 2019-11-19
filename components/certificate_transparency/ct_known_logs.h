// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_KNOWN_LOGS_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_KNOWN_LOGS_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace certificate_transparency {

struct CTLogInfo {
  // The DER-encoded SubjectPublicKeyInfo for the log.  Note that this is not
  // the same as a "log ID": a log ID is the SHA-256 hash of this value.
  const char* const log_key;
  // The length, in bytes, of |log_key|.
  const size_t log_key_length;
  // The user-friendly log name.
  // Note: This will not be translated.
  const char* const log_name;
};

// Returns information about all known logs, which includes those that are
// presently qualified for inclusion and logs which were previously qualified,
// but have since been disqualified. To determine the status of a given log
// (via its log ID), use |GetDisqualifiedLogs()|.
std::vector<CTLogInfo> GetKnownLogs();

// Returns the log IDs of all logs that are operated by Google, sorted.  The log
// ID is the SHA-256 hash of the log's |log_key|.
std::vector<std::string> GetLogsOperatedByGoogle();

// Returns pairs of (log ID, disqualification date) for all disqualified logs,
// where the log ID is the SHA-256 hash of the log's |log_key|).  The list is
// sorted by log ID.  The disqualification date is expressed as seconds since
// the Unix epoch.
//
// Any SCTs that are embedded in certificates issued after the disqualification
// date should not be trusted, nor contribute to any uniqueness or freshness
// requirements.
std::vector<std::pair<std::string, base::TimeDelta>> GetDisqualifiedLogs();

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_KNOWN_LOGS_H_
