// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SCANNER_MATCHER_UTIL_H_
#define CHROME_CHROME_CLEANER_SCANNER_MATCHER_UTIL_H_

#include <set>
#include <string>
#include <vector>

#include "base/strings/string16.h"

namespace base {
class FilePath;
}  // namespace base

namespace chrome_cleaner {

class SignatureMatcherAPI;

bool IsKnownFileByDigest(const base::FilePath& path,
                         const SignatureMatcherAPI* signature_matcher,
                         const char* const digests[],
                         size_t digests_length);

// A pair of filesize and digest. The filesize is used to avoid computing the
// digest of a file.
struct FileDigestInfo {
  const char* const digest;
  size_t filesize;
};

// Check whether the checksum (sha256) of a given file is part of a sorted
// array of |FileDigestInfo|.
bool IsKnownFileByDigestInfo(const base::FilePath& path,
                             const SignatureMatcherAPI* signature_matcher,
                             const FileDigestInfo* digests,
                             size_t digests_length);

bool IsKnownFileByOriginalFilename(const base::FilePath& path,
                                   const SignatureMatcherAPI* signature_matcher,
                                   const base::char16* const names[],
                                   size_t names_length);

bool IsKnownFileByCompanyName(const base::FilePath& path,
                              const SignatureMatcherAPI* signature_matcher,
                              const base::char16* const names[],
                              size_t names_length);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SCANNER_MATCHER_UTIL_H_
