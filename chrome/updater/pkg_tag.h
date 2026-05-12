// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PKG_TAG_H_
#define CHROME_UPDATER_PKG_TAG_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/containers/span.h"
#include "chrome/updater/certificate_tag.h"

namespace updater::tagging {

// Reads the tag string from the end of a MacOS .pkg file. The provided region
// of the file must cover the tag (if any) through the end of the file.
//
// Returns the extracted tag string, excluding the magic signature and length
// bytes. If no tag is found, returns an empty string.
std::string ReadTagFromPkg(base::span<const uint8_t> tail);

// Parses the contents of an entire MacOS .pkg file into a `BinaryInterface`.
// The data in `contents`, which must cover the entire file, is copied into
// the constructed object.
//
// If `contents` is not a valid macOS .pkg file or it contains a tag with
// length bytes that point outside the tag zone, this will return either
// `nullptr` or an object that appears correct but produces useless results.
// Otherwise, it returns a `unique_ptr` to a valid `BinaryInterface`.
std::unique_ptr<BinaryInterface> CreatePkgBinary(
    base::span<const uint8_t> contents);

}  // namespace updater::tagging

#endif  // CHROME_UPDATER_PKG_TAG_H_
