// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PKG_TAG_H_
#define CHROME_UPDATER_PKG_TAG_H_

#include <cstdint>
#include <iosfwd>
#include <memory>

#include "base/containers/span.h"
#include "chrome/updater/certificate_tag.h"

namespace updater::tagging {

enum class PkgTagError {
  kBufferTooSmall,
  kInvalidXarMagic,
  kInvalidXarHeader,
  kInvalidTocSize,
  kTocDecompressionFailed,
  kInvalidTocXml,
  kInvalidDescriptorNode,
  kIntegerOverflow,
  kPayloadOutOfBounds,
  kInvalidTrailer,
};

std::ostream& operator<<(std::ostream& os, PkgTagError error);

// Parses the contents of an entire MacOS .pkg (XAR) file into a
// `BinaryInterface`. The data in `contents`, which must cover the entire file,
// is copied into the constructed object.
//
// If `contents` is not a valid macOS .pkg (XAR) file, this will return
// `nullptr`. Otherwise, it returns a `unique_ptr` to a valid `BinaryInterface`.
std::unique_ptr<BinaryInterface> CreatePkgBinary(
    base::span<const uint8_t> contents);

}  // namespace updater::tagging

#endif  // CHROME_UPDATER_PKG_TAG_H_
