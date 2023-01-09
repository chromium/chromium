// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/signature_matcher.h"

#include <windows.h>

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/scanner/matcher_util.h"

namespace chrome_cleaner {

bool SignatureMatcher::MatchFileDigestInfo(
    const base::FilePath& path,
    size_t* filesize,
    std::string* digest,
    const FileDigestInfo& digest_info) const {
  DCHECK(filesize);
  DCHECK(digest);

  if (*filesize == 0) {
    DCHECK(digest->empty());
    int64_t local_filesize = 0;
    if (!base::GetFileSize(path, &local_filesize)) {
      LOG(ERROR) << "Failed to get filesize of path: '" << SanitizePath(path)
                 << "'.";
      return false;
    }
    *filesize = local_filesize;
  } else {
    int64_t dcheck_filesize = 0;
    DCHECK(base::GetFileSize(path, &dcheck_filesize) &&
           *filesize == static_cast<size_t>(dcheck_filesize));
  }

  if (digest_info.filesize != *filesize)
    return false;

  if (digest->empty()) {
    if (!ComputeSHA256DigestOfPath(path, digest)) {
      digest->clear();
      LOG(ERROR) << "Unable to compute digest SHA256 for: '"
                 << SanitizePath(path) << "'.";
      return false;
    }
  } else {
    std::string dcheck_digest;
    DCHECK(ComputeSHA256DigestOfPath(path, &dcheck_digest) &&
           *digest == dcheck_digest);
  }

  return digest->compare(digest_info.digest) == 0;
}

bool SignatureMatcher::ComputeSHA256DigestOfPath(const base::FilePath& path,
                                                 std::string* digest) const {
  // TODO(pmbureau): Add caching to avoid recomputing digests.
  // TODO(pmbureau): Add synchronization to avoid multiple file operation.
  // TODO(pmbureau): Add copy of locked files.
  return chrome_cleaner::ComputeSHA256DigestOfPath(path, digest);
}

// TODO(pmbureau): Add a unittest for this function.
bool SignatureMatcher::RetrieveVersionInformation(
    const base::FilePath& path,
    VersionInformation* information) const {
  DCHECK(information);
  // TODO(pmbureau): Add caching to avoid recomputing information.

  std::unique_ptr<FileVersionInfo> version(
      FileVersionInfo::CreateFileVersionInfo(path));
  if (!version.get())
    return false;

  information->company_name = base::AsWString(version->company_name());
  information->original_filename =
      base::AsWString(version->original_filename());
  return true;
}

}  // namespace chrome_cleaner
