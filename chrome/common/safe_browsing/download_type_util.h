// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SAFE_BROWSING_DOWNLOAD_TYPE_UTIL_H_
#define CHROME_COMMON_SAFE_BROWSING_DOWNLOAD_TYPE_UTIL_H_

#include "base/files/file_path.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {
namespace download_type_util {

// Returns the DownloadType of the file at |path|. This function is only valid
// for paths that satisfy IsSupportedBinaryFile() above.
ClientDownloadRequest::DownloadType GetDownloadType(const base::FilePath& file);

}  // namespace download_type_util
}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_DOWNLOAD_TYPE_UTIL_H_
