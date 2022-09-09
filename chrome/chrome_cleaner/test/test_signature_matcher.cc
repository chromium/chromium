// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_signature_matcher.h"

namespace chrome_cleaner {

TestSignatureMatcher::TestSignatureMatcher() : scan_error_(false) {}
TestSignatureMatcher::~TestSignatureMatcher() = default;

bool TestSignatureMatcher::MatchFileDigestInfo(
    const base::FilePath& path,
    size_t* filesize,
    std::string* digest,
    const FileDigestInfo& digest_info) const {
  DCHECK(filesize);
  DCHECK(digest);
  base::AutoLock lock(lock_);
  const auto& matched_digest_info =
      matched_digest_info_.find(NormalizePath(path));
  if (matched_digest_info == matched_digest_info_.end())
    return false;
  if (*filesize == 0) {
    DCHECK(digest->empty());
    *filesize = matched_digest_info->second.filesize;
    *digest = matched_digest_info->second.digest;
  } else {
    DCHECK_EQ(matched_digest_info->second.filesize, *filesize);
    DCHECK_EQ(matched_digest_info->second.digest, *digest);
  }
  return matched_digest_info->second.filesize == digest_info.filesize &&
         matched_digest_info->second.digest == digest_info.digest;
}

bool TestSignatureMatcher::ComputeSHA256DigestOfPath(
    const base::FilePath& path,
    std::string* digest) const {
  DCHECK(digest);
  base::AutoLock lock(lock_);
  const auto& matched_digest = matched_digests_.find(NormalizePath(path));
  if (matched_digest == matched_digests_.end())
    return false;
  *digest = matched_digest->second;
  return true;
}

bool TestSignatureMatcher::RetrieveVersionInformation(
    const base::FilePath& path,
    VersionInformation* information) const {
  DCHECK(information);
  base::AutoLock lock(lock_);

  const auto& matched_information =
      matched_version_informations_.find(NormalizePath(path));
  if (matched_information == matched_version_informations_.end())
    return false;
  *information = matched_information->second;
  return true;
}
}  // namespace chrome_cleaner
