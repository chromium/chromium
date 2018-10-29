// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/quarantine/test_support.h"

#include <windows.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/win/scoped_handle.h"
#include "components/download/quarantine/common_win.h"

namespace download {

namespace {

bool ZoneIdentifierPresentForFile(const base::FilePath& path) {
  const DWORD kShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  base::FilePath::StringType zone_identifier_path =
      path.value() + kZoneIdentifierStreamSuffix;
  base::win::ScopedHandle file(
      ::CreateFile(zone_identifier_path.c_str(), GENERIC_READ, kShare, nullptr,
                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!file.IsValid())
    return false;

  // The zone identifier contents is expected to be:
  // "[ZoneTransfer]\r\nZoneId=3\r\n". The actual ZoneId can be different. A
  // buffer of 32 bytes is sufficient for verifying the contents.
  std::vector<char> zone_identifier_contents_buffer(32);
  DWORD actual_length = 0;
  if (!::ReadFile(file.Get(), &zone_identifier_contents_buffer.front(),
                  zone_identifier_contents_buffer.size(), &actual_length,
                  nullptr))
    return false;
  zone_identifier_contents_buffer.resize(actual_length);

  std::string zone_identifier_contents(zone_identifier_contents_buffer.begin(),
                                       zone_identifier_contents_buffer.end());

  std::vector<base::StringPiece> lines =
      base::SplitStringPiece(zone_identifier_contents, "\n",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return lines.size() > 1 && lines[0] == "[ZoneTransfer]" &&
         lines[1].find("ZoneId=") == 0;
}

}  // namespace

bool IsFileQuarantined(const base::FilePath& file,
                       const GURL& source_url,
                       const GURL& referrer_url) {
  return ZoneIdentifierPresentForFile(file);
}

}  // namespace download
