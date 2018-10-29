// Copyright 2018 The Chromium Authors. All rights reserved.
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

// A macro that puts the function name into the logging stream.  This is a
// drop-in replacement for the LOG macro.

#define LOGFN(LEVEL) SYSLOG(LEVEL) << __FUNCTION__ << ": "

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_LOGGING_H_
