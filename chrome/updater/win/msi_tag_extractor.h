// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_MSI_TAG_EXTRACTOR_H_
#define CHROME_UPDATER_WIN_MSI_TAG_EXTRACTOR_H_

#include <string>

#include "base/containers/flat_map.h"

namespace base {
class FilePath;
}

namespace updater {

// Extracts a tag from the end of the MSI `filename`. Returns the tag as a
// key/value map.
//
// The tag specification for MSI files is as follows:
//   - The tag area begins with a magic signature 'Gact2.0Omaha'.
//   - The next 2 bytes are the tag string length in big endian.
//   - Then comes the tag string in the format "key1=value1&key2=value2".
//     Both the key and the value are alphanumeric ASCII strings.
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
base::flat_map<std::string, std::string> ExtractTagMap(
    const base::FilePath& filename);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_MSI_TAG_EXTRACTOR_H_
