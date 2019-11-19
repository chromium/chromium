// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_COMPONENT_UNPACKER_H_
#define COMPONENTS_UPDATE_CLIENT_COMPONENT_UNPACKER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/update_client_errors.h"

namespace crx_file {
enum class VerifierFormat;
}

namespace update_client {

class CrxInstaller;
class ComponentPatcher;
class Patcher;
class Unzipper;

// In charge of unpacking the component CRX package and verifying that it is
// well formed and the cryptographic signature is correct.
//
// This class should be used only by the component updater. It is inspired by
// and overlaps with code in the extension's SandboxedUnpacker.
// The main differences are:
// - The public key hash is full SHA256.
// - Does not use a sandboxed unpacker. A valid component is fully trusted.
// - The manifest can have different attributes and resources are not
//   transcoded.
//
// If the CRX is a delta CRX, the flow is:
//   [ComponentUpdater]      [ComponentPatcher]
//   Unpack
//     \_ Verify
//     \_ Unzip
//     \_ BeginPatching ---> DifferentialUpdatePatch
//                             ...
//   EndPatching <------------ ...
//     \_ EndUnpacking
//
// For a full CRX, the flow is:
//   [ComponentUpdater]
//   Unpack
//     \_ Verify
//     \_ Unzip
//     \_ BeginPatching
//          |
//          V
//   EndPatching
//     \_ EndUnpacking
//
// In both cases, if there is an error at any point, the remaining steps will
// be skipped and EndUnpacking will be called.
class ComponentUnpacker : public base::RefCountedThreadSafe<ComponentUnpacker> {
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

  using Callback = base::OnceCallback<void(const Result& result)>;

  // Constructs an unpacker for a specific component unpacking operation.
  // |pk_hash| is the expected public developer key's SHA256 hash. If empty,
  // the unpacker accepts any developer key. |path| is the current location
  // of the CRX.
  ComponentUnpacker(const std::vector<uint8_t>& pk_hash,
                    const base::FilePath& path,
                    scoped_refptr<CrxInstaller> installer,
                    std::unique_ptr<Unzipper> unzipper,
                    scoped_refptr<Patcher> patcher,
                    crx_file::VerifierFormat crx_format);

  // Begins the actual unpacking of the files. May invoke a patcher and the
  // component installer if the package is a differential update.
  // Calls |callback| with the result.
  void Unpack(Callback callback);

 private:
  friend class base::RefCountedThreadSafe<ComponentUnpacker>;

  virtual ~ComponentUnpacker();

  // The first step of unpacking is to verify the file. Returns false if an
  // error is encountered, the file is malformed, or the file is incorrectly
  // signed.
  bool Verify();

  // The second step of unpacking is to unzip. Returns false if an early error
  // is encountered.
  bool BeginUnzipping();
  void EndUnzipping(bool error);

  // The third step is to optionally patch files - this is a no-op for full
  // (non-differential) updates. This step is asynchronous. Returns false if an
  // error is encountered.
  bool BeginPatching();
  void EndPatching(UnpackerError error, int extended_error);

  // The final step is to do clean-up for things that can't be tidied as we go.
  // If there is an error at any step, the remaining steps are skipped and
  // EndUnpacking is called. EndUnpacking is responsible for calling the
  // callback provided in Unpack().
  void EndUnpacking();

  std::vector<uint8_t> pk_hash_;
  base::FilePath path_;
  base::FilePath unpack_path_;
  base::FilePath unpack_diff_path_;
  bool is_delta_;
  scoped_refptr<ComponentPatcher> patcher_;
  scoped_refptr<CrxInstaller> installer_;
  Callback callback_;
  std::unique_ptr<Unzipper> unzipper_;
  scoped_refptr<Patcher> patcher_tool_;
  crx_file::VerifierFormat crx_format_;
  UnpackerError error_;
  int extended_error_;
  std::string public_key_;

  DISALLOW_COPY_AND_ASSIGN(ComponentUnpacker);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_COMPONENT_UNPACKER_H_
