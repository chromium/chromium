// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_KEY_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_KEY_H_

#include <cstdint>
#include <ostream>
#include <string>

#include "base/time/time.h"

namespace segmentation_platform {

// The SignalKey is used for identifying a particular record in the
// SignalDatabase. The format is defined in go/chrome-segmentation-storage-mvp.
//
// It provides functionality to convert to and from a binary format. The format
// for this binary key must never change on a single device, so an internal
// representation for this, SignalKeyInternal, is used.
//
// The SignalKey is not meant to be used by external clients in any way,
// and should be considered as an internal implementation detail of the
// SignalDatabase.
//
// The binary representation of the key does not store any resolution smaller
// than seconds, so any constructed SignalKey is immediately stripped of
// resolutions smaller than 1 second to ensure a SignalKey which has been
// converted to a binary key and back again matches the original SignalKey.
//
// Since the binary representation is not human readable, the struct also
// supports being streamed or by calling ToDebugString(), which will make it be
// presented using this format:
// {kind=..., name_hash=..., range_start=..., range_start=...} which is useful
// for debugging.
//
// The binary representation of a key can be lexicographically compared. The
// fields are in the following order: kind, name_hash, range_start, range_end.
class SignalKey {
 public:
  enum Kind {
    UNKNOWN = 0,
    USER_ACTION = 1,
    HISTOGRAM_VALUE = 2,
    HISTOGRAM_ENUM = 3,
  };

  SignalKey(Kind kind,
            uint64_t name_hash,
            base::Time range_start,
            base::Time range_end);
  SignalKey();
  ~SignalKey();

  // Whether this object has been initialized and does not contain unknown data.
  bool IsValid() const;

  Kind kind() const { return kind_; }
  uint64_t name_hash() const { return name_hash_; }
  // The smallest resolution for range_start() and range_end() is 1 second, so
  // any fraction of a second is dropped.
  const base::Time& range_start() const { return range_start_; }
  const base::Time& range_end() const { return range_end_; }

  // A machine readable representation of the SignalKey.
  // See ToDebugString and operator<< implementation for a human readable
  // format.
  std::string ToBinary() const;
  // Parses a machine readable representation of a SignalKeyInternal into
  // a SignalKey. Returns whether the conversion succeeded.
  [[nodiscard]] static bool FromBinary(const std::string& input,
                                       SignalKey* output);
  // The SignalKey prefix in binary format.
  std::string GetPrefixInBinary() const;
  // Returns a human readable representation of the SignalKey.
  std::string ToDebugString() const;

  // Allow SignalKey to be a key in STL containers.
  bool operator<(const SignalKey& other) const;

 private:
  // The type of record this key refers to.
  Kind kind_;
  // The name of the sample identifier, for example the hash of the histogram
  // or user action.
  uint64_t name_hash_;
  // The first record timestamp this key refers to.
  base::Time range_start_;
  // The latest record timestamp this key refers to.
  base::Time range_end_;
};

std::ostream& operator<<(std::ostream& os, const SignalKey& key);

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_KEY_H_
