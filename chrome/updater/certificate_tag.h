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

namespace updater::tagging {

class BinaryInterface {
 public:
  // tag returns the complete embedded tag, if any, including the tag header.
  // It may or may not include trailing tag padding, depending on the tag
  // format.
  virtual std::optional<std::vector<uint8_t>> tag() const = 0;

  // Returns an updated version of the binary with the provided `tag`, or
  // `nullopt` on error. If the binary already contains a tag then it will be
  // replaced.
  //
  // `tag` is injected directly into the binary with no further processing.
  // Therefore, it must already be a valid Omaha 4 tag (including signature
  // and length), to be useful; if it isn't, it will be written into the
  // binary anyway over any existing tag or injected as though it is a tag.
  //
  // If writing over an existing tag, the `tag` argument is presumed to be no
  // longer than the existing tag patch space, since the total patch space
  // length cannot be verified. If it is too long, file content after the
  // tag patch space may be overwritten with the excess bytes from `tag`,
  // which is likely to silently corrupt the file.
  //
  // If writing a new tag, `tag` is inserted as the entire tag and patch
  // space. Tags (including blank tags) intended to be patched on-the-fly by the
  // download server must therefore be padded by the caller with null bytes to
  // the size of the patch space (see `kMaxTagStringBytes` in tag.cc).
  virtual std::optional<std::vector<uint8_t>> SetTag(
      base::span<const uint8_t> tag) = 0;

  virtual ~BinaryInterface() = default;
};

std::unique_ptr<BinaryInterface> CreatePEBinary(
    base::span<const uint8_t> contents);
std::unique_ptr<BinaryInterface> CreateMSIBinary(
    base::span<const uint8_t> contents);

}  // namespace updater::tagging

#endif  // CHROME_UPDATER_CERTIFICATE_TAG_H_
