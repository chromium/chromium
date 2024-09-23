// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_KEY_INTERNAL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_KEY_INTERNAL_H_

#include <stdint.h>
#include <ostream>
#include <string>
#include <type_traits>

namespace segmentation_platform {

// The SignalKeyInternal is used for identifying a particular record in the
// SignalDatabase. The format is defined in go/chrome-segmentation-storage-mvp.
// It is important that on a single device, this representation never changes,
// so steps are taken to make sure it is safe to make assumptions about the
// memory layout of the key.
//
// The SignalKeyInternal is not meant to be used by external clients in any way,
// and should be considered as an internal implementation detail of the
// SignalDatabase.
//
// Since the binary representation is not human readable, the struct also
// supports being streamed or by calling SignalKeyInternalToDebugString(),
// which will make it be presented using this format:
// {{kind:name_hash}:time_range_end_sec:time_range_start_sec} which is useful
// for debugging.
//
// The structure of the key is on the following format:
// +----+---------------------------+
// |kind| (PADDING)                 |
// |char| char[7]                   |
// +----+---------------------------+
// | name_hash                      |
// | uint64_t                       |
// +--------------------------------+
// | time_range_end_sec             |
// | int64_t                        |
// +--------------------------------+
// | time_range_start_sec           |
// | int64_t                        |
// +--------------------------------+
//
// Even though SignalKeyInternal is serialized/deserialized using big endian
// writer and reader respectively, it still cannot contain any implicit padding
// since implicit padding bytes could contain any value and prevent two
// otherwise equal binary keys from comparing as equal.
//
// The binary format is created using big endian, which means that the binary
// key can be used to do lexicographical comparisons in its binary format
// (represented as std::string). The same is true for prefix-based lookups using
// the SignalKeyInternal::Prefix.
// The fields are in the following order: kind, name_hash, range_start,
// range_end (SignalKey::Prefix only contains the first two fields).
struct SignalKeyInternal {
  struct Prefix {
    // The type of record this key refers to.
    char kind{};

    // This padding is required to be able to guarantee a standard layout.
    const char padding[7]{};

    // The name of the sample identifier, for example the hash of the histogram
    // or user action.
    uint64_t name_hash{};
  };
  Prefix prefix;

  // The latest record timestamp this key refers to.
  int64_t time_range_end_sec{};
  // The first record timestamp this key refers to.
  int64_t time_range_start_sec{};
};

// Verify that we can safely make assumptions about the memory layout.
// It is important to recognize that if you change how the key is laid out,
// old keys will no longer be usable, and since this key is persisted to disk,
// this is something that should be avoided.
// See https://en.cppreference.com/w/cpp/named_req/StandardLayoutType
static_assert(std::is_standard_layout_v<SignalKeyInternal>,
              "SignalKeyInternal must have a standard layout.");

// Ensure there is no implicit padding.
static_assert(std::has_unique_object_representations_v<SignalKeyInternal>,
              "SignalKeyInternal must have a unique object representation.");

// A machine readable representation of the SignalKeyInternal.
// See ToDebugString and operator<< implementation for a human readable
// format.
std::string SignalKeyInternalToBinary(const SignalKeyInternal& input);
// Parses a machine readable representation of a SignalKeyInternal into
// a SignalKeyInternal. Returns whether the conversion succeeded.
[[nodiscard]] bool SignalKeyInternalFromBinary(const std::string& input,
                                               SignalKeyInternal* output);
// Returns a human readable representation of the SignalKeyInternal.
std::string SignalKeyInternalToDebugString(const SignalKeyInternal& input);

// A machine readable representation of the SignalKeyInternal::Prefix.
// See ToDebugString and operator<< implementation for a human readable
// format.
std::string SignalKeyInternalPrefixToBinary(
    const SignalKeyInternal::Prefix& input);
// Parses a machine readable representation of a SignalKeyInternal::Prefix into
// a SignalKeyInternal::Prefix. Returns whether the conversion succeeded.
[[nodiscard]] bool SignalKeyInternalPrefixFromBinary(
    const std::string& input,
    SignalKeyInternal::Prefix* output);
// Returns a human readable representation of the SignalKeyInternal::Prefix.
std::string SignalKeyInternalPrefixToDebugString(
    const SignalKeyInternal::Prefix& input);

// The following streaming operators make it easy to get a human readable
// version of the SignalKeyInternal and SignalKeyInternal::Prefix.
std::ostream& operator<<(std::ostream& os, const SignalKeyInternal& key);
std::ostream& operator<<(std::ostream& os,
                         const SignalKeyInternal::Prefix& prefix);

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_KEY_INTERNAL_H_
