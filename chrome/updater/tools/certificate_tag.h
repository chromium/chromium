// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TOOLS_CERTIFICATE_TAG_H_
#define CHROME_UPDATER_TOOLS_CERTIFICATE_TAG_H_

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace tools {

// Binary represents a Windows PE binary and provides functions to extract and
// set data outside of the signed area (called a "tag"). This allows a binary to
// contain arbitrary data without invalidating any Authenticode signature.
class Binary {
 public:
  Binary(const Binary&);
  ~Binary();

  // Parse a signed, Windows PE binary. Note that the returned structure
  // contains pointers into the given data.
  static absl::optional<Binary> Parse(base::span<const uint8_t> binary);

  // tag returns the embedded tag, if any.
  const absl::optional<base::span<const uint8_t>>& tag() const;

  // SetTag returns an updated version of the binary that contains the given
  // tag, or |nullopt| on error. If the binary already contains a tag then it
  // will be replaced.
  absl::optional<std::vector<uint8_t>> SetTag(
      base::span<const uint8_t> tag) const;

 private:
  Binary();

  // ParseTag attempts to parse out the tag. It returns false on parse error or
  // true on success. If successful, it sets |tag_|.
  bool ParseTag();

  // binary_ contains the whole input binary.
  base::span<const uint8_t> binary_;

  // content_info_ contains the |WIN_CERTIFICATE| structure.
  base::span<const uint8_t> content_info_;

  // tag_ contains the embedded tag, or |nullopt| if there isn't one.
  absl::optional<base::span<const uint8_t>> tag_;

  // attr_cert_offset_ is the offset in the file where the |WIN_CERTIFICATE|
  // structure appears. (This is the last structure in the file.)
  size_t attr_cert_offset_ = 0;

  // certs_size_offset_ is the offset in the file where the u32le size of the
  // |WIN_CERTIFICATE| structure is embedded in an |IMAGE_DATA_DIRECTORY|.
  size_t certs_size_offset_ = 0;
};

}  // namespace tools
}  // namespace updater

#endif  // CHROME_UPDATER_TOOLS_CERTIFICATE_TAG_H_
