// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_DIGEST_VERIFIER_H_
#define CHROME_CHROME_CLEANER_OS_DIGEST_VERIFIER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

namespace chrome_cleaner {

// This class verifies whether a file on disk is known, and if it is, whether
// the file's digest matches the expected digest for that file.
class DigestVerifier : public base::RefCounted<DigestVerifier> {
 public:
  // Returns an instance of DigestVerifier, or nullptr if initialization
  // failed. This will load a list of digests from the "TEXT" resource with ID
  // |resource_id|, which is expected to be a FileDigests proto (see
  // file_digest.proto).
  static scoped_refptr<DigestVerifier> CreateFromResource(int resource_id);

  // Returns an instance of DigestVerifier that can test if other files have
  // the same basename and digest as |file|. Returns nullptr if it can't
  // calculate the digest of |file|.
  static scoped_refptr<DigestVerifier> CreateFromFile(
      const base::FilePath& file);

  // Returns true if |file| has a name that matches the name of one of the known
  // files, and if that file's digest also matches the expected digest as
  // specified in the proto used when creating this instance.
  bool IsKnownFile(const base::FilePath& file) const;

  std::vector<base::FilePath::StringType> GetKnownFileNames() const;

 private:
  friend class base::RefCounted<DigestVerifier>;

  DigestVerifier();
  ~DigestVerifier();

  bool InitializeFromResource(int resource_id);
  bool InitializeFromFile(const base::FilePath& file);

  // Maps a file's basename to the expected digest for that file.
  std::unordered_map<base::FilePath::StringType, std::string> digests_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_DIGEST_VERIFIER_H_
