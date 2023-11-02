// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_PATH_UTILS_H_
#define CHROMECAST_BASE_PATH_UTILS_H_

#include <string>

#include "base/files/file_path.h"

namespace chromecast {

// If |path| is relative, returns the path created by prepending HOME directory
// to |path| (e.g. {HOME}/|path|). If |path| is absolute, returns |path|.
base::FilePath GetHomePath(const base::FilePath& path);
base::FilePath GetHomePathASCII(const std::string& path);

// If |path| is relative, returns the path created by prepending BIN directory
// to |path| (e.g. {BIN}/|path|). If |path| is absolute, returns |path|.
base::FilePath GetBinPath(const base::FilePath& path);
base::FilePath GetBinPathASCII(const std::string& path);

// If |path| is relative, returns the path created by prepending TMP directory
// to |path| (e.g. {TMP}/|path|). If |path| is absolute, returns |path|.
base::FilePath GetTmpPath(const base::FilePath& path);
base::FilePath GetTmpPathASCII(const std::string& path);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_PATH_UTILS_H_
