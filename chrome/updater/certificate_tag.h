// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// `certificate_tag` manipulates "tags" in Authenticode-signed Windows binaries.
//
// This library handles "superfluous certificate" tags. These are mock
// certificates, inserted into the PKCS#7 certificate chain, that can contain
// arbitrary data in extensions. Since they are also not hashed when verifying
// signatures, that data can also be changed without invalidating it.
//
// The library supports PE32 exe files and MSI files.

#ifndef CHROME_UPDATER_CERTIFICATE_TAG_H_
#define CHROME_UPDATER_CERTIFICATE_TAG_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"

namespace updater {
namespace tagging {

class BinaryInterface {
 public:
  // tag returns the embedded tag, if any.
  virtual std::optional<std::vector<uint8_t>> tag() const = 0;

  // Returns an updated version of the binary with the provided `tag`, or
  // `nullopt` on error. If the binary already contains a tag then it will be
  // replaced.
  virtual std::optional<std::vector<uint8_t>> SetTag(
      base::span<const uint8_t> tag) = 0;

  virtual ~BinaryInterface() = default;
};

std::unique_ptr<BinaryInterface> CreatePEBinary(
    base::span<const uint8_t> contents);
std::unique_ptr<BinaryInterface> CreateMSIBinary(
    base::span<const uint8_t> contents);

}  // namespace tagging
}  // namespace updater

#endif  // CHROME_UPDATER_CERTIFICATE_TAG_H_
