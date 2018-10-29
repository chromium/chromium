// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/download_type_util.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/safe_browsing/features.h"

namespace safe_browsing {
namespace download_type_util {

ClientDownloadRequest::DownloadType GetDownloadType(
    const base::FilePath& file) {
  // TODO(nparker): Put all of this logic into the FileTypePolicies
  // protobuf.
  if (file.MatchesExtension(FILE_PATH_LITERAL(".apk")))
    return ClientDownloadRequest::ANDROID_APK;
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".crx")))
    return ClientDownloadRequest::CHROME_EXTENSION;
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".zip")))
    // DownloadProtectionService doesn't send a ClientDownloadRequest for ZIP
    // files unless they contain either executables or archives. The resulting
    // DownloadType is either ZIPPED_EXECUTABLE or ZIPPED_ARCHIVE respectively.
    // This function will return ZIPPED_EXECUTABLE for ZIP files as a
    // placeholder. The correct DownloadType will be determined based on the
    // result of analyzing the ZIP file.
    return ClientDownloadRequest::ZIPPED_EXECUTABLE;
  else if (base::FeatureList::IsEnabled(kInspectDownloadedRarFiles) &&
           file.MatchesExtension(FILE_PATH_LITERAL(".rar")))
    // See the comment for .zip files.
    return ClientDownloadRequest::RAR_COMPRESSED_EXECUTABLE;
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".dmg")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".img")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".iso")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".pkg")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".mpkg")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".smi")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".app")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".cdr")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".dmgpart")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".dvdr")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".dart")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".dc42")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".diskcopy42")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".imgpart")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".ndif")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".udif")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".toast")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".sparsebundle")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".sparseimage")))
    return ClientDownloadRequest::MAC_EXECUTABLE;
  else if (FileTypePolicies::GetInstance()->IsArchiveFile(file))
    return ClientDownloadRequest::ARCHIVE;
  return ClientDownloadRequest::WIN_EXECUTABLE;
}

}  // namespace download_type_util
}  // namespace safe_browsing
