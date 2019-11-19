// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/matcher_util.h"

#include <stdint.h>

#include <cctype>
#include <memory>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/scanner/signature_matcher_api.h"
#include "chrome/chrome_cleaner/strings/string_util.h"

namespace chrome_cleaner {

bool IsKnownFileByDigest(const base::FilePath& path,
                         const SignatureMatcherAPI* signature_matcher,
                         const char* const digests[],
                         size_t digests_length) {
  DCHECK(signature_matcher);
  if (base::DirectoryExists(path) || !base::PathExists(path))
    return false;

  std::string path_digest;
  if (!signature_matcher->ComputeSHA256DigestOfPath(path, &path_digest)) {
    PLOG(ERROR) << "Can't compute file digest: '" << SanitizePath(path) << "'.";
    return false;
  }

  for (size_t index = 0; index < digests_length; ++index) {
    const char* expected_digest = digests[index];
    DCHECK(expected_digest);
    DCHECK_EQ(expected_digest, base::ToUpperASCII(expected_digest));
    DCHECK_EQ(64UL, strlen(expected_digest));
    if (path_digest.compare(expected_digest) == 0)
      return true;
  }

  return false;
}

bool IsKnownFileByDigestInfo(const base::FilePath& fullpath,
                             const SignatureMatcherAPI* signature_matcher,
                             const FileDigestInfo* digests,
                             size_t digests_length) {
  DCHECK(signature_matcher);
  DCHECK(digests);

  if (base::DirectoryExists(fullpath) || !base::PathExists(fullpath))
    return false;

  size_t filesize = 0;
  std::string digest;
  for (size_t index = 0; index < digests_length; ++index) {
    if (signature_matcher->MatchFileDigestInfo(fullpath, &filesize, &digest,
                                               digests[index])) {
      return true;
    }
  }
  return false;
}

bool IsKnownFileByOriginalFilename(const base::FilePath& path,
                                   const SignatureMatcherAPI* signature_matcher,
                                   const base::char16* const names[],
                                   size_t names_length) {
  DCHECK(signature_matcher);
  DCHECK(names);
  VersionInformation version_information;
  if (base::DirectoryExists(path) || !base::PathExists(path) ||
      !signature_matcher->RetrieveVersionInformation(path,
                                                     &version_information)) {
    return false;
  }

  for (size_t i = 0; i < names_length; ++i) {
    if (String16EqualsCaseInsensitive(version_information.original_filename,
                                      names[i])) {
      return true;
    }
  }
  return false;
}

bool IsKnownFileByCompanyName(const base::FilePath& path,
                              const SignatureMatcherAPI* signature_matcher,
                              const base::char16* const names[],
                              size_t names_length) {
  DCHECK(signature_matcher);
  DCHECK(names);
  VersionInformation version_information;
  if (base::DirectoryExists(path) || !base::PathExists(path) ||
      !signature_matcher->RetrieveVersionInformation(path,
                                                     &version_information)) {
    return false;
  }

  for (size_t i = 0; i < names_length; ++i) {
    if (String16EqualsCaseInsensitive(version_information.company_name,
                                      names[i])) {
      return true;
    }
  }
  return false;
}

}  // namespace chrome_cleaner
