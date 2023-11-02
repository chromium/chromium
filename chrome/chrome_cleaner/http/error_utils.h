// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_HTTP_ERROR_UTILS_H_
#define CHROME_CHROME_CLEANER_HTTP_ERROR_UTILS_H_

#include <windows.h>
#include <wtypes.h>
#include <ostream>

namespace common {

// Logs HRESULTs verbosely, with the error code and human-readable error
// text if available.
class LogHr {
 public:
  explicit LogHr(HRESULT hr) : hr_(hr) {}

 private:
  HRESULT hr_;
  friend std::ostream& operator<<(std::ostream&, const LogHr&);
};

std::ostream& operator<<(std::ostream& os, const LogHr& hr);

// Logs Windows errors verbosely, with the error code and human-readable error
// text if available.
class LogWe {
 public:
  LogWe() : we_(::GetLastError()) {}
  explicit LogWe(DWORD we) : we_(we) {}

 private:
  DWORD we_;
  friend std::ostream& operator<<(std::ostream&, const LogWe&);
};

std::ostream& operator<<(std::ostream& os, const LogWe& we);

}  // namespace common

#endif  // CHROME_CHROME_CLEANER_HTTP_ERROR_UTILS_H_
