// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/download_type_util.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/files/file_path.h"
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
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".rar")))
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
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".pdf")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".doc")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".docx")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".docm")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".docb")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".dot")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".dotm")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".dotx")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xls")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xlsb")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xlt")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xlm")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xlsx")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xldm")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xltx")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xltm")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xlsb")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xla")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xlam")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xll")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xlw")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".ppt")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".pot")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".pps")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".pptx")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".pptm")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".potx")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".potm")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".ppam")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".ppsx")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".ppsm")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".sldx")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".xldm")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".rtf")))
    return ClientDownloadRequest::DOCUMENT;

  return ClientDownloadRequest::WIN_EXECUTABLE;
}

}  // namespace download_type_util
}  // namespace safe_browsing
