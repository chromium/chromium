// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_SAFARI_IMPORTER_UTILS_H_
#define CHROME_COMMON_IMPORTER_SAFARI_IMPORTER_UTILS_H_

#include <stdint.h>

namespace base {
class FilePath;
}

// Does this user account have a Safari Profile and if so, what items
// are supported?
// in: library_dir - ~/Library or a standin for testing purposes.
// out: services_supported - the service supported for import.
// Returns true if we can import the Safari profile.
bool SafariImporterCanImport(const base::FilePath& library_dir,
                             uint16_t* services_supported);

#endif  // CHROME_COMMON_IMPORTER_SAFARI_IMPORTER_UTILS_H_
