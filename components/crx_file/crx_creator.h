// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRX_FILE_CRX_CREATOR_H_
#define COMPONENTS_CRX_FILE_CRX_CREATOR_H_

#include <string>

namespace base {
class FilePath;
}  // namespace base

namespace crypto {
class RSAPrivateKey;
}  // namespace crypto

namespace crx_file {

enum class CreatorResult {
  OK,  // The CRX file was successfully created.
  ERROR_SIGNING_FAILURE,
  ERROR_FILE_NOT_READABLE,
  ERROR_FILE_NOT_WRITABLE,
  ERROR_FILE_WRITE_FAILURE,
};

// Similar to `Create` method but also injects `verified_contents` into the
// header.
CreatorResult CreateCrxWithVerifiedContentsInHeader(
    const base::FilePath& output_path,
    const base::FilePath& zip_path,
    crypto::RSAPrivateKey* signing_key,
    const std::string& verified_contents);

// Create a CRX3 file at |output_path|, using the contents of the ZIP archive
// located at |zip_path| and signing with (and deriving the CRX ID from)
// |signing_key|.
CreatorResult Create(const base::FilePath& output_path,
                     const base::FilePath& zip_path,
                     crypto::RSAPrivateKey* signing_key);

}  // namespace crx_file

#endif  // COMPONENTS_CRX_FILE_CRX_CREATOR_H_
