// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PUFFIN_COMPONENT_UNPACKER_H_
#define COMPONENTS_UPDATE_CLIENT_PUFFIN_COMPONENT_UNPACKER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/update_client/update_client_errors.h"

namespace crx_file {
enum class VerifierFormat;
}

namespace update_client {

class Unzipper;

// Unpacks the component CRX package and verifies that it is
// well formed and the cryptographic signature is correct.
//
// This class is only used by the component updater. It is inspired by
// and overlaps with code in the extension's SandboxedUnpacker.
// The main differences are:
// - The public key hash is SHA256.
// - Does not use a sandboxed unpacker. A valid component is fully trusted.
// - The manifest can have different attributes and resources are not
//   transcoded.
//
// This is an updated version of ComponentUnpacker that leverages the new
// Puffin-based puffpatch CRX-diff format, rather than the legacy
// courgette/bsdiff per-file CRXD format. Puffin patches the CRX before
// unpacking, changing the order of operations such that patching needs to occur
// before verifying and unpacking. Unlike the original implementation, by the
// time we unpack, the patching has already occurred.
class PuffinComponentUnpacker
    : public base::RefCountedThreadSafe<PuffinComponentUnpacker> {
 public:
  // Contains the result of the unpacking.
  struct Result {
    Result();

    // Unpack error: 0 indicates success.
    UnpackerError error = UnpackerError::kNone;

    // Additional error information, such as errno or last error.
    int extended_error = 0;

    // Path of the unpacked files if the unpacking was successful.
    base::FilePath unpack_path;

    // The extracted public key of the package if the unpacking was successful.
    std::string public_key;
  };

  PuffinComponentUnpacker(const PuffinComponentUnpacker&) = delete;
  PuffinComponentUnpacker& operator=(const PuffinComponentUnpacker&) = delete;

  // Begins the actual unpacking of the files. Calls `callback` with the result.
  static void Unpack(const std::vector<uint8_t>& pk_hash,
                     const base::FilePath& path,
                     std::unique_ptr<Unzipper> unzipper,
                     crx_file::VerifierFormat crx_format,
                     base::OnceCallback<void(const Result& result)> callback);

 private:
  friend class base::RefCountedThreadSafe<PuffinComponentUnpacker>;

  // Constructs an unpacker for a specific component unpacking operation.
  // `pk_hash` is the expected public developer key's SHA256 hash. If empty,
  // the unpacker accepts any developer key. `path` is the current location
  // of the CRX.
  PuffinComponentUnpacker(
      const std::vector<uint8_t>& pk_hash,
      const base::FilePath& path,
      std::unique_ptr<Unzipper> unzipper,
      crx_file::VerifierFormat crx_format,
      base::OnceCallback<void(const Result& result)> callback);

  virtual ~PuffinComponentUnpacker();

  // The first step of unpacking is to verify the file. Triggers
  // `BeginUnzipping` if successful. Triggers `EndUnpacking` if an early error
  // is encountered.
  void Verify();

  // The next step of unpacking is to unzip. Triggers `EndUnzipping` if
  // successful. Triggers `EndUnpacking` if an early error is encountered.
  void BeginUnzipping();
  void EndUnzipping(bool error);

  // The final step is to do clean-up for things that can't be tidied as we go.
  // If there is an error at any step, the remaining steps are skipped and
  // `EndUnpacking` is called. `EndUnpacking` is responsible for calling the
  // callback provided in `Unpack`.
  void EndUnpacking(UnpackerError error, int extended_error);

  std::vector<uint8_t> pk_hash_;
  base::FilePath path_;
  std::unique_ptr<Unzipper> unzipper_;
  crx_file::VerifierFormat crx_format_;
  base::OnceCallback<void(const Result& result)> callback_;
  base::FilePath unpack_path_;
  std::string public_key_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PUFFIN_COMPONENT_UNPACKER_H_
