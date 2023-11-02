// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SCANNER_SIGNATURE_MATCHER_H_
#define CHROME_CHROME_CLEANER_SCANNER_SIGNATURE_MATCHER_H_

#include "chrome/chrome_cleaner/scanner/signature_matcher_api.h"

namespace chrome_cleaner {

class SignatureMatcher : public SignatureMatcherAPI {
 public:
  SignatureMatcher() = default;
  ~SignatureMatcher() override = default;

  // SignatureMatcherAPI implementation.
  bool MatchFileDigestInfo(const base::FilePath& path,
                           size_t* filesize,
                           std::string* digest,
                           const FileDigestInfo& digest_info) const override;
  bool ComputeSHA256DigestOfPath(const base::FilePath& path,
                                 std::string* digest) const override;
  bool RetrieveVersionInformation(
      const base::FilePath& path,
      VersionInformation* information) const override;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SCANNER_SIGNATURE_MATCHER_H_
