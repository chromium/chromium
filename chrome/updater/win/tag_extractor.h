// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_TAG_EXTRACTOR_H_
#define CHROME_UPDATER_WIN_TAG_EXTRACTOR_H_

#include <string>

namespace updater {

// The character encoding of tag in the binary. The tag is stored differently
// depending on the encoding:
//
// UTF-8:
//   Encoded length - the length in bytes of the tag is stored as a
//   big-endian uint16_t right after the magic string.
//
//   Format: <Start Magic><Tag length><Tag>
//
// UTF-16:
//   End magic string - the tag is enclosed within two different magic
//   strings. The tag is stored as big-endian.
//
//   Format: <Start Magic><Tag><End Magic>
//
enum class TagEncoding {
  kUtf8,
  kUtf16,
};

// Returns the UTF-8 encoding of the tag, or an empty string if not found.
std::string ExtractTagFromFile(const std::wstring& path, TagEncoding encoding);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_TAG_EXTRACTOR_H_
