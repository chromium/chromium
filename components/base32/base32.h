// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BASE32_BASE32_H_
#define COMPONENTS_BASE32_BASE32_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"

namespace base32 {

// LINT.IfChange(Base32EncodePolicy)
enum class Base32EncodePolicy {
  // Include the trailing padding in the output, when necessary.
  INCLUDE_PADDING,
  // Omit trailing padding in the output. Such an output will not be decodable
  // unless |input.size()| is known by the decoder. Its size is guaranteed to be
  // |ceil(input.size() * 8.0 / 5.0)|.
  OMIT_PADDING
};
// LINT.ThenChange(//components/base32/base32.rs:Base32EncodePolicy)

// Separate implementations exposed for testing.
// TODO(crbug.com/536936880): Migrate to Rust implementation.
namespace internal {
// Legacy C++ implementation.
std::string Base32EncodeCpp(base::span<const uint8_t> input,
                            Base32EncodePolicy policy);
std::vector<uint8_t> Base32DecodeCpp(std::string_view input);

// Rust implementation.
std::string Base32EncodeRust(base::span<const uint8_t> input,
                             Base32EncodePolicy policy);
std::vector<uint8_t> Base32DecodeRust(std::string_view input);
}  // namespace internal

// Encodes the |input| string in base32, defined in RFC 4648:
// https://tools.ietf.org/html/rfc4648#section-5
//
// The |policy| defines whether padding should be included or omitted from the
// encoded output.
std::string Base32Encode(
    base::span<const uint8_t> input,
    Base32EncodePolicy policy = Base32EncodePolicy::INCLUDE_PADDING);

// Decodes the |input| string piece from base32. Returns an empty vector on
// error, including if |input| is empty.
// This function only decodes exactly if the preimage was an integer number of
// bytes.
std::vector<uint8_t> Base32Decode(std::string_view input);

}  // namespace base32

#endif  // COMPONENTS_BASE32_BASE32_H_
