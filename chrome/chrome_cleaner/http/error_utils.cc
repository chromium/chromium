// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/http/error_utils.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/win/atl.h"
#include "base/win/shlwapi.h"

namespace common {

std::ostream& operator<<(std::ostream& os, const LogHr& hr) {
  // Looks up the human-readable system message for the HRESULT code
  // and since we're not passing any params to FormatMessage, we don't
  // want inserts expanded.
  const DWORD kFlags =
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  char error_text[4096] = {'\0'};
  ::FormatMessageA(kFlags, 0, hr.hr_, 0, error_text, std::size(error_text),
                   NULL);
  std::string error(error_text);
  base::TrimWhitespaceASCII(error, base::TRIM_ALL, &error);

  return os << "[hr=0x" << std::hex << hr.hr_ << ", msg=" << error << "]";
}

std::ostream& operator<<(std::ostream& os, const LogWe& we) {
  // Looks up the human-readable system message for the Windows error code
  // and since we're not passing any params to FormatMessage, we don't
  // want inserts expanded.
  const DWORD kFlags =
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  char error_text[4096] = {'\0'};
  ::FormatMessageA(kFlags, 0, we.we_, 0, error_text, std::size(error_text),
                   NULL);
  std::string error(error_text);
  base::TrimWhitespaceASCII(error, base::TRIM_ALL, &error);

  return os << "[we=" << we.we_ << ", msg=" << error << "]";
}

}  // namespace common
