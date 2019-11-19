// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "components/services/quarantine/common.h"
#include "components/services/quarantine/common_win.h"
#include "components/services/quarantine/test_support.h"

namespace quarantine {

namespace {

bool ZoneIdentifierPresentForFile(const base::FilePath& path,
                                  const GURL source_url,
                                  const GURL referrer_url) {
  const DWORD kShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  base::FilePath::StringType zone_identifier_path =
      path.value() + kZoneIdentifierStreamSuffix;
  base::win::ScopedHandle file(
      ::CreateFile(zone_identifier_path.c_str(), GENERIC_READ, kShare, nullptr,
                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!file.IsValid())
    return false;

  // During testing, the zone identifier is expected to be under this limit.
  std::vector<char> zone_identifier_contents_buffer(4096);
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
  if (lines.size() < 2 || lines[0] != "[ZoneTransfer]" != 0)
    return false;

  base::StringPiece found_zone_id;
  base::StringPiece found_host_url;
  base::StringPiece found_referrer_url;

  // Note that we don't try too hard to parse the zone identifier here. This is
  // a test. If Windows starts adding whitespace or doing anything fancier than
  // ASCII, then we'd have to update this.
  for (const auto& line : lines) {
    if (line.starts_with("ZoneId="))
      found_zone_id = line.substr(7);
    else if (line.starts_with("HostUrl="))
      found_host_url = line.substr(8);
    else if (line.starts_with("ReferrerUrl="))
      found_referrer_url = line.substr(12);
  }

  return !found_zone_id.empty() &&
         (source_url.is_empty() ||
          SanitizeUrlForQuarantine(source_url).spec() == found_host_url) &&
         (referrer_url.is_empty() ||
          SanitizeUrlForQuarantine(referrer_url).spec() == found_referrer_url);
}

}  // namespace

bool IsFileQuarantined(const base::FilePath& file,
                       const GURL& source_url,
                       const GURL& referrer_url) {
  if (base::win::GetVersion() >= base::win::Version::WIN10)
    return ZoneIdentifierPresentForFile(file, source_url, referrer_url);
  else
    return ZoneIdentifierPresentForFile(file, GURL::EmptyGURL(),
                                        GURL::EmptyGURL());
}

}  // namespace quarantine
