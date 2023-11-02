// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SCANNER_SIGNATURE_MATCHER_API_H_
#define CHROME_CHROME_CLEANER_SCANNER_SIGNATURE_MATCHER_API_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"

namespace chrome_cleaner {

struct FileDigestInfo;

// This structure holds version information about an executable.
// (see: base/file_version_info.h)
struct VersionInformation {
  std::wstring company_name;
  std::wstring original_filename;
};

// This class is used as a wrapper around the signature matcher calls. The
// purpose of the signature matcher is to match a sequence of bytes against
// a set of known signals and report the name of the rules that matches.
class SignatureMatcherAPI {
 public:
  virtual ~SignatureMatcherAPI() {}

  // Compare the file's digest info and return true on a successful match.
  // |filesize| & |digest| are used if they are not initialized (e.g., 0 for
  // |filesize| and an empty string for |digest|), and otherwise they are set
  // using |path|. This is mainly so that tests can overload it.
  virtual bool MatchFileDigestInfo(const base::FilePath& path,
                                   size_t* filesize,
                                   std::string* digest,
                                   const FileDigestInfo& digest_info) const = 0;

  // Compute the SHA256 checksum of |path| and store it as base16 into |digest|.
  // Return true on success.
  virtual bool ComputeSHA256DigestOfPath(const base::FilePath& path,
                                         std::string* digest) const = 0;

  // Retrieve version information fields of a given executable |path|. Return
  // false if an error occurred.
  virtual bool RetrieveVersionInformation(
      const base::FilePath& path,
      VersionInformation* information) const = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SCANNER_SIGNATURE_MATCHER_API_H_
