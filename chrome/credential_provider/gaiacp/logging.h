// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_LOGGING_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_LOGGING_H_

#include <ostream>

#include "base/syslog_logging.h"
#include "base/win/windows_types.h"

namespace credential_provider {

// An ostream manipulator that writes an HRESULT in hex to the stream.
class putHR {
 public:
  explicit putHR(HRESULT hr) : hr_(hr) {}
  friend std::ostream& operator<<(std::ostream& stream, const putHR& o);

 private:
  HRESULT hr_;
};

// COMPACT_GOOGLE_LOG_EX_VERBOSE is defined only for GCPW as INFO event log
// category.
#ifdef COMPACT_GOOGLE_LOG_EX_VERBOSE
#undef COMPACT_GOOGLE_LOG_EX_VERBOSE
#endif

#define COMPACT_GOOGLE_LOG_EX_VERBOSE(ClassName, ...)               \
  ::logging::ClassName(__FILE__, __LINE__, ::logging::LOGGING_INFO, \
                       ##__VA_ARGS__)

// A helper macro which checks if the message should be logged based on log
// level.
#define LOG_ENABLED(LEVEL) \
  (::logging::LOGGING_##LEVEL >= logging::GetMinLogLevel())

// A macro that puts the function name into the logging stream.  This is a
// drop-in replacement for the LOG macro.
#define LOGFN(LEVEL) \
  LAZY_STREAM((SYSLOG(LEVEL) << __FUNCTION__ << ": "), LOG_ENABLED(LEVEL))

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_LOGGING_H_
