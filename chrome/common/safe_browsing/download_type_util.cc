// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/download_type_util.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"

namespace safe_browsing {
namespace download_type_util {

ClientDownloadRequest::DownloadType GetDownloadType(
    const base::FilePath& file) {
  base::FilePath::StringType ext = file.Extension();

  // TODO(nparker): Put all of this logic into the FileTypePolicies
  // protobuf.
  if (base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".apk")))
    return ClientDownloadRequest::ANDROID_APK;
  else if (base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".crx")))
    return ClientDownloadRequest::CHROME_EXTENSION;
  else if (base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".zip")))
    // DownloadProtectionService doesn't send a ClientDownloadRequest for ZIP
    // files unless they contain either executables or archives. The resulting
    // DownloadType is either ZIPPED_EXECUTABLE or ZIPPED_ARCHIVE respectively.
    // This function will return ZIPPED_EXECUTABLE for ZIP files as a
    // placeholder. The correct DownloadType will be determined based on the
    // result of analyzing the ZIP file.
    return ClientDownloadRequest::ZIPPED_EXECUTABLE;
  else if (base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".rar")))
    // See the comment for .zip files.
    return ClientDownloadRequest::RAR_COMPRESSED_EXECUTABLE;
  else if (base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".dmg")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".img")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".iso")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".pkg")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".mpkg")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".smi")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".app")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".cdr")) ||
           base::EqualsCaseInsensitiveASCII(ext,
                                            FILE_PATH_LITERAL(".dmgpart")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".dvdr")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".dart")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".dc42")) ||
           base::EqualsCaseInsensitiveASCII(ext,
                                            FILE_PATH_LITERAL(".diskcopy42")) ||
           base::EqualsCaseInsensitiveASCII(ext,
                                            FILE_PATH_LITERAL(".imgpart")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".ndif")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".udif")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".toast")) ||
           base::EqualsCaseInsensitiveASCII(
               ext, FILE_PATH_LITERAL(".sparsebundle")) ||
           base::EqualsCaseInsensitiveASCII(ext,
                                            FILE_PATH_LITERAL(".sparseimage")))
    return ClientDownloadRequest::MAC_EXECUTABLE;
  else if (FileTypePolicies::GetInstance()->IsArchiveFile(file))
    return ClientDownloadRequest::ARCHIVE;
  else if (base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".pdf")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".doc")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".docx")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".docm")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".docb")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".dot")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".dotm")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".dotx")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xls")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xlsb")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xlt")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xlm")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xlsx")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xldm")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xltx")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xltm")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xla")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xlam")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xll")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".xlw")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".ppt")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".pot")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".pps")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".pptx")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".pptm")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".potx")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".potm")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".ppam")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".ppsx")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".ppsm")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".sldx")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".rtf")) ||
           base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".wll")))
    return ClientDownloadRequest::DOCUMENT;
  else if (base::EqualsCaseInsensitiveASCII(ext, FILE_PATH_LITERAL(".7z")))
    return ClientDownloadRequest::SEVEN_ZIP_COMPRESSED_EXECUTABLE;

  return ClientDownloadRequest::WIN_EXECUTABLE;
}

}  // namespace download_type_util
}  // namespace safe_browsing
