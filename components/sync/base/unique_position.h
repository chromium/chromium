// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_UNIQUE_POSITION_H_
#define COMPONENTS_SYNC_BASE_UNIQUE_POSITION_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace sync_pb {
class UniquePosition;
}

namespace syncer {

class ClientTagHash;

// A class to represent positions.
//
// Valid UniquePosition objects have the following properties:
//
//  - a < b and b < c implies a < c (transitivity);
//  - exactly one of a < b, b < a and a = b holds (trichotomy);
//  - if a < b, there is a UniquePosition such that a < x < b (density);
//  - there are UniquePositions x and y such that x < a < y (unboundedness);
//  - if a and b were constructed with different unique suffixes, then a != b.
//
// As long as all UniquePositions used to sort a list were created with unique
// suffixes, then if any item changes its position in the list, only its
// UniquePosition value has to change to represent the new order, and all other
// values can stay the same.
//
// Note that the unique suffixes must be exactly |kSuffixLength| bytes long.
//
// The cost for all these features is potentially unbounded space usage.  In
// practice, however, most ordinals should be not much longer than the suffix.
class UniquePosition {
 public:
  // The suffix must be exactly the specified length, otherwise unique suffixes
  // are not sufficient to guarantee unique positions (because prefix + suffix
  // == p + refixsuffix).
  static constexpr size_t kSuffixLength = 28;
  static constexpr size_t kCompressBytesThreshold = 128;

  using Suffix = std::array<uint8_t, kSuffixLength>;

  static bool IsValidSuffix(const Suffix& suffix);
  static bool IsValidBytes(const std::string& bytes);

  // Returns a valid, but mostly random suffix.
  // Avoid using this; it can lead to inconsistent sort orderings if misused.
  static Suffix RandomSuffix();

  // Returns a valid suffix based on the given client tag hash.
  static Suffix GenerateSuffix(const ClientTagHash& client_tag_hash);

  // Converts from a 'sync_pb::UniquePosition' protobuf to a UniquePosition.
  // This may return an invalid position if the parsing fails.
  static UniquePosition FromProto(const sync_pb::UniquePosition& proto);

  // Creates a position with the given suffix.  Ordering among positions created
  // from this function is the same as that of the integer parameters that were
  // passed in. |suffix| must be a valid suffix with length |kSuffixLength|.
  static UniquePosition FromInt64(int64_t i, const Suffix& suffix);

  // Returns a valid position. Its ordering is not defined. |suffix| must be a
  // valid suffix with length |kSuffixLength|.
  static UniquePosition InitialPosition(const Suffix& suffix);

  // Returns positions compare smaller than, greater than, or between the input
  // positions. |suffix| must be a valid suffix with length |kSuffixLength|.
  static UniquePosition Before(const UniquePosition& x, const Suffix& suffix);
  static UniquePosition After(const UniquePosition& x, const Suffix& suffix);
  static UniquePosition Between(const UniquePosition& before,
                                const UniquePosition& after,
                                const Suffix& suffix);

  // Creates an empty, invalid value.
  UniquePosition();

  // Type is copyable and movable.
  UniquePosition(const UniquePosition&) = default;
  UniquePosition(UniquePosition&&) = default;
  UniquePosition& operator=(const UniquePosition&) = default;
  UniquePosition& operator=(UniquePosition&&) = default;

  bool LessThan(const UniquePosition& other) const;
  bool Equals(const UniquePosition& other) const;

  // Serializes the position's internal state to a protobuf.
  sync_pb::UniquePosition ToProto() const;

  // Serializes the protobuf representation of this object as a string.
  void SerializeToString(std::string* blob) const;

  // Returns a human-readable representation of this item's internal state.
  std::string ToDebugString() const;

  // Returns the suffix.
  Suffix GetSuffixForTest() const;

  bool IsValid() const;

  // Returns memory usage estimate.
  size_t EstimateMemoryUsage() const;

 private:
  friend class UniquePositionTest;

  // Returns a string X such that (X ++ |suffix|) < |str|.
  // |str| must be a trailing substring of a valid ordinal.
  // |suffix| must be a valid unique suffix.
  static std::string FindSmallerWithSuffix(const std::string& str,
                                           const Suffix& suffix);
  // Returns a string X such that (X ++ |suffix|) > |str|.
  // |str| must be a trailing substring of a valid ordinal.
  // |suffix| must be a valid unique suffix.
  static std::string FindGreaterWithSuffix(const std::string& str,
                                           const Suffix& suffix);
  // Returns a string X such that |before| < (X ++ |suffix|) < |after|.
  // |before| and after must be a trailing substrings of valid ordinals.
  // |suffix| must be a valid unique suffix.
  static std::string FindBetweenWithSuffix(const std::string& before,
                                           const std::string& after,
                                           const Suffix& suffix);

  // Expects a run-length compressed string as input.  For internal use only.
  explicit UniquePosition(const std::string& compressed);

  // Expects an uncompressed prefix and suffix as input.  The |suffix| parameter
  // must be a suffix of |uncompressed|.  For internal use only.
  UniquePosition(const std::string& uncompressed, const Suffix& suffix);

  // Implementation of an order-preserving run-length compression scheme.
  static std::string Compress(const std::string& input);
  static std::string CompressImpl(const std::string& input);
  static std::string Uncompress(const std::string& compressed);
  static bool IsValidCompressed(const std::string& str);

  // The position value after it has been run through the custom compression
  // algorithm.  See Compress() and Uncompress() functions above.
  std::string compressed_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_UNIQUE_POSITION_H_
