// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_SIGNATURE_MATCHER_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_SIGNATURE_MATCHER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_version_info.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/scanner/matcher_util.h"
#include "chrome/chrome_cleaner/scanner/signature_matcher_api.h"

namespace chrome_cleaner {

// A signature matcher implementation used for testing.
class TestSignatureMatcher : public SignatureMatcherAPI {
 public:
  TestSignatureMatcher() : scan_error_(false) {}

  bool MatchFileDigestInfo(const base::FilePath& path,
                           size_t* filesize,
                           std::string* digest,
                           const FileDigestInfo& digest_info) const override {
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

  bool ComputeSHA256DigestOfPath(const base::FilePath& path,
                                 std::string* digest) const override {
    DCHECK(digest);
    base::AutoLock lock(lock_);
    const auto& matched_digest = matched_digests_.find(NormalizePath(path));
    if (matched_digest == matched_digests_.end())
      return false;
    *digest = matched_digest->second;
    return true;
  }

  bool RetrieveVersionInformation(
      const base::FilePath& path,
      VersionInformation* information) const override {
    DCHECK(information);
    base::AutoLock lock(lock_);

    const auto& matched_information =
        matched_version_informations_.find(NormalizePath(path));
    if (matched_information == matched_version_informations_.end())
      return false;
    *information = matched_information->second;
    return true;
  }

  void Reset() {
    base::AutoLock lock(lock_);
    scan_error_ = false;
    matched_basenames_.clear();
    matched_digests_.clear();
    computed_digests_.clear();
    matched_version_informations_.clear();
  }

  void MatchDigest(const base::FilePath& path, const char* digest) {
    base::AutoLock lock(lock_);
    matched_digests_[NormalizePath(path)] = digest;
  }

  void MatchDigestInfo(const base::FilePath& path,
                       const char* digest,
                       size_t filesize) {
    base::AutoLock lock(lock_);
    const base::FilePath normalized_path = NormalizePath(path);
    matched_digest_info_[normalized_path].digest = digest;
    matched_digest_info_[normalized_path].filesize = filesize;
  }

  void MatchVersionInformation(const base::FilePath& path,
                               const VersionInformation& information) {
    matched_version_informations_[NormalizePath(path)] = information;
  }

  void MatchBaseName(const base::FilePath& path,
                     const std::string& identifier) {
    base::AutoLock lock(lock_);
    matched_basenames_[NormalizePath(path).BaseName()] = identifier;
  }

  void ForceScanFailure() {
    base::AutoLock lock(lock_);
    scan_error_ = true;
  }

 private:
  struct TestFileDigestInfo {
    std::string digest;
    size_t filesize;
  };

  bool scan_error_;

  // Map paths to signature identifiers. When the given path is scanned, the
  // mapped identifier is returned.
  std::map<base::FilePath, std::string> matched_basenames_;
  std::map<base::FilePath, std::string> matched_digests_;
  std::map<base::FilePath, TestFileDigestInfo> matched_digest_info_;
  std::map<base::FilePath, VersionInformation> matched_version_informations_;

  // Keep track of calls and parameters done.
  std::set<base::FilePath> computed_digests_;

  // Must be held for all access to all data since |ScanFile| can be called
  // from other threads.
  mutable base::Lock lock_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_SIGNATURE_MATCHER_H_
