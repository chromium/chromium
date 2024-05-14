// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/offline_url_utils.h"

#include "base/hash/md5.h"
#include "base/notreached.h"

namespace {
const base::FilePath::CharType kOfflineDirectory[] =
    FILE_PATH_LITERAL("Offline");
const base::FilePath::CharType kMainPageFileName[] =
    FILE_PATH_LITERAL("page.html");
const base::FilePath::CharType kPDFFileName[] = FILE_PATH_LITERAL("file.pdf");
}  // namespace

namespace reading_list {

base::FilePath OfflineRootDirectoryPath(const base::FilePath& profile_path) {
  return profile_path.Append(kOfflineDirectory);
}

std::string OfflineURLDirectoryID(const GURL& url) {
  return base::MD5String(url.spec());
}

base::FilePath OfflineURLDirectoryAbsolutePath(
    const base::FilePath& profile_path,
    const GURL& url) {
  return OfflineURLAbsolutePathFromRelativePath(
      profile_path, base::FilePath::FromUTF8Unsafe(OfflineURLDirectoryID(url)));
}

base::FilePath OfflinePagePath(const GURL& url, OfflineFileType type) {
  base::FilePath directory =
      base::FilePath::FromUTF8Unsafe(OfflineURLDirectoryID(url));
  switch (type) {
    case OFFLINE_TYPE_HTML:
      return directory.Append(kMainPageFileName);
    case OFFLINE_TYPE_PDF:
      return directory.Append(kPDFFileName);
  }
  NOTREACHED_IN_MIGRATION();
  return base::FilePath();
}

base::FilePath OfflineURLAbsolutePathFromRelativePath(
    const base::FilePath& profile_path,
    const base::FilePath& relative_path) {
  return OfflineRootDirectoryPath(profile_path).Append(relative_path);
}
}
