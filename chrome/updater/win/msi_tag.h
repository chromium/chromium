// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// +-----------------------------------------------------------------+
// Utilities for reading and writing tags to MSI files.
// +-----------------------------------------------------------------+
//
// The tag specification for MSI files is as follows:
//   - The tag area begins with a magic signature 'Gact2.0Omaha'.
//   - The next 2 bytes are the tag string length in big endian.
//   - Then comes the tag string in the format "key1=value1&key2=value2".
//   - The key is alphanumeric, the value allows special characters such as '*'.
//
// A sample layout:
// +-------------------------------------+
// ~    ..............................   ~
// |    ..............................   |
// |    Other parts of the MSI file      |
// +-------------------------------------+
// | Start of the certificate             |
// ~    ..............................   ~
// ~    ..............................   ~
// | Magic signature 'Gact2.0Omaha'      | Tag starts
// | Tag length (2 bytes in big-endian)) |
// | tag string                          |
// +-------------------------------------+
//
// A real example (an MSI file tagged with 'brand=CDCD&key2=Test'):
// +-----------------------------------------------------------------+
// |  G   a   c   t   2   .   0   O   m   a   h   a  0x0 0x14 b   r  |
// |  a   n   d   =   C   D   C   D   &   k   e   y   2   =   T   e  |
// |  s   t                                                          |
// +-----------------------------------------------------------------+

#ifndef CHROME_UPDATER_WIN_MSI_TAG_H_
#define CHROME_UPDATER_WIN_MSI_TAG_H_

#include <string>

#include "chrome/updater/tag.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}

namespace updater {

// Extracts a tag from the end of the MSI `filename`.
absl::optional<tagging::TagArgs> ExtractTagArgs(const base::FilePath& filename);

// Copies `in_file` to `out_file`, and then writes the `tag` to the end of
// `out_file`.
bool WriteTagString(const base::FilePath& in_file,
                    const base::FilePath& out_file,
                    const std::string& tag_string);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_MSI_TAG_H_
