// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Stolen from chrome/browser/component_updater/component_unpacker.h

#ifndef CHROME_CHROME_CLEANER_COMPONENTS_COMPONENT_UNPACKER_H_
#define CHROME_CHROME_CLEANER_COMPONENTS_COMPONENT_UNPACKER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"

namespace chrome_cleaner {

// In charge of unpacking the component CRX package and verifying that it is
// well formed and the cryptographic signature is correct.
//
// This class is inspired by and overlaps with code in the extension's
// SandboxedUnpacker.
// The main differences are:
// - The public key hash is full SHA256.
// - Does not use a sandboxed unpacker. A valid component is fully trusted.
class ComponentUnpacker {
 public:
  // Constructs an unpacker for a specific component unpacking operation.
  // |pk_hash| is the expected public key SHA256 hash. |path| is the current
  // location of the CRX.
  ComponentUnpacker(const std::vector<uint8_t>& pk_hash,
                    const base::FilePath& path);
  virtual ~ComponentUnpacker();

  // Unpack the file to the provided folder. Return true on success.
  bool Unpack(const base::FilePath& ouput_folder);

 private:
  // The first step of unpacking is to verify the file. Return false if an
  // error is encountered, the file is malformed, or the file is incorrectly
  // signed.
  bool Verify();

  std::vector<uint8_t> pk_hash_;
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(ComponentUnpacker);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_COMPONENTS_COMPONENT_UNPACKER_H_
