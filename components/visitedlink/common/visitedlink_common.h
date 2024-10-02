// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITEDLINK_COMMON_VISITEDLINK_COMMON_H_
#define COMPONENTS_VISITEDLINK_COMMON_VISITEDLINK_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/visitedlink/core/visited_link.h"

class GURL;

namespace net {
class SchemefulSite;
}

namespace url {
class Origin;
}

namespace visitedlink {

// number of bytes in the salt
#define LINK_SALT_LENGTH 8

// A multiprocess-safe database of the visited links for the browser. There
// should be exactly one process that has write access (implemented by
// VisitedLinkWriter), while all other processes should be read-only
// (implemented by VisitedLinkReader). These other processes add links by
// calling the writer process to add them for it. The writer may also notify the
// readers to replace their table when the table is resized.
//
// IPC is not implemented in these classes. This is done through callback
// functions supplied by the creator of these objects to allow more flexibility,
// especially for testing.
//
// This class defines the common base for these others. We implement accessors
// for looking things up in the hash table, and for computing hash values and
// fingerprints. Both the writer and the reader inherit from this, and add their
// own code to set up and change these values as their design requires. The
// reader pretty much just sets up the shared memory and saves the pointer. The
// writer does a lot of work to manage the table, reading and writing it to and
// from disk, and resizing it when it gets too full.
//
// To ask whether a page is in history, we compute a 64-bit fingerprint of the
// URL. This URL is hashed and we see if it is in the URL hashtable. If it is,
// we consider it visited. Otherwise, it is unvisited. Note that it is possible
// to get collisions, which is the penalty for not storing all URL strings in
// memory (which could get to be more than we want to have in memory). We use
// a salt value for the links on one computer so that an attacker can not
// manually create a link that causes a collision.
class VisitedLinkCommon {
 public:
  // A number that identifies the URL.
  typedef uint64_t Fingerprint;
  typedef std::vector<Fingerprint> Fingerprints;

  // A hash value of a fingerprint
  typedef int32_t Hash;

  // A fingerprint or hash value that does not exist
  static const Fingerprint null_fingerprint_;
  static const Hash null_hash_;

  VisitedLinkCommon();

  VisitedLinkCommon(const VisitedLinkCommon&) = delete;
  VisitedLinkCommon& operator=(const VisitedLinkCommon&) = delete;

  virtual ~VisitedLinkCommon();

  // Returns the fingerprint for the given URL.
  Fingerprint ComputeURLFingerprint(std::string_view canonical_url) const {
    return ComputeURLFingerprint(canonical_url, salt_);
  }

  // Looks up the given key in the table. Returns true if found. Does not
  // modify the hashtable.
  bool IsVisited(std::string_view canonical_url) const;
  bool IsVisited(const GURL& url) const;
  // To check if a link is visited in a partitioned table, callers MUST
  // provide <link url, top-level site, frame origin> AND the origin salt.
  bool IsVisited(const VisitedLink& link, uint64_t salt);
  bool IsVisited(const GURL& link_url,
                 const net::SchemefulSite& top_level_site,
                 const url::Origin& frame_origin,
                 uint64_t salt);
  bool IsVisited(Fingerprint fingerprint) const;

#ifdef UNIT_TEST
  // Returns statistics about DB usage
  void GetUsageStatistics(int32_t* table_size,
                          VisitedLinkCommon::Fingerprint** fingerprints) {
    *table_size = table_length_;
    *fingerprints = hash_table_;
  }
#endif

 protected:
  // This structure is at the beginning of the unpartitioned shared memory so
  // that the readers can get stats on the table.
  struct SharedHeader {
    // see goes into table_length_
    uint32_t length;

    // goes into salt_
    uint8_t salt[LINK_SALT_LENGTH];

    // Padding to ensure the Fingerprint table is aligned. Without this, reading
    // from the table causes unaligned reads.
    uint8_t padding[4];
  };

  static_assert(sizeof(SharedHeader) % alignof(Fingerprint) == 0,
                "Fingerprint must be aligned when placed after SharedHeader");

  // This structure is at the beginning of the partitioned shared memory so that
  // the readers can get stats on the table. We do not include a salt in this
  // shared header, as the salts are generated per-origin for the partitioned
  // table. Readers will receive their salts via the
  // VisitedLinkNavigationThrottle or via the VisitedLinkNotificationSink IPC.
  struct PartitionedSharedHeader {
    // see goes into table_length_
    uint32_t length;

    // Padding to ensure the Fingerprint table is aligned. Without this, reading
    // from the table causes unaligned reads.
    uint8_t padding[4];
  };

  static_assert(
      sizeof(PartitionedSharedHeader) % alignof(Fingerprint) == 0,
      "Fingerprint must be aligned when placed after PartitionedSharedHeader");

  // Returns the fingerprint at the given index into the URL table. This
  // function should be called instead of accessing the table directly to
  // contain endian issues.
  Fingerprint FingerprintAt(int32_t table_offset) const {
    if (!hash_table_)
      return null_fingerprint_;
    return hash_table_[table_offset];
  }

  // Computes the fingerprint of the given canonical URL. It is static so the
  // same algorithm can be re-used by the table rebuilder, so you will have to
  // pass the salt as a parameter. See the non-static version above if you
  // want to use the current class' salt.
  static Fingerprint ComputeURLFingerprint(
      std::string_view canonical_url,
      const uint8_t salt[LINK_SALT_LENGTH]);

  // Computes the fingerprint of the given VisitedLink using the provided
  // per-origin `salt`.
  static Fingerprint ComputePartitionedFingerprint(const VisitedLink& link,
                                                   uint64_t salt);

  // Computes the fingerprint of the given partition key. Used when constructing
  // the partitioned :visited link hashtable.
  static Fingerprint ComputePartitionedFingerprint(
      const GURL& link_url,
      const net::SchemefulSite& top_level_site,
      const url::Origin& frame_origin,
      uint64_t salt);

  // Computes the hash value of the given fingerprint, this is used as a lookup
  // into the hashtable.
  static Hash HashFingerprint(Fingerprint fingerprint, int32_t table_length) {
    if (table_length == 0)
      return null_hash_;
    return static_cast<Hash>(fingerprint % table_length);
  }
  // Uses the current hashtable.
  Hash HashFingerprint(Fingerprint fingerprint) const {
    return HashFingerprint(fingerprint, table_length_);
  }

  // pointer to the first item
  // May temporarily point to an old unmapped region during update.
  raw_ptr<VisitedLinkCommon::Fingerprint,
          DisableDanglingPtrDetection | AllowPtrArithmetic>
      hash_table_ = nullptr;

  // the number of items in the hash table
  int32_t table_length_ = 0;

  // salt used for each URL when computing the fingerprint
  uint8_t salt_[LINK_SALT_LENGTH];
};

}  // namespace visitedlink

#endif  // COMPONENTS_VISITEDLINK_COMMON_VISITEDLINK_COMMON_H_
