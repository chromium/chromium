// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PATTERN_INDEX_CLOSED_HASH_MAP_H_
#define COMPONENTS_URL_PATTERN_INDEX_CLOSED_HASH_MAP_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"

namespace url_pattern_index {

template <typename KeyType_, typename Hasher_>
class SimpleQuadraticProber;

template <typename KeyType_, typename ValueType_, typename Prober_>
class ClosedHashMap;

// The default Prober used by the HashMap.
template <typename KeyType_, typename Hasher_>
using DefaultProber = SimpleQuadraticProber<KeyType_, Hasher_>;

// The default HashMap data structure meant to be used. For more fine-grained
// control one can use ClosedHashMap with their own Prober.
template <typename KeyType_,
          typename ValueType_,
          typename Hasher_ = std::hash<KeyType_>>
using HashMap =
    ClosedHashMap<KeyType_, ValueType_, DefaultProber<KeyType_, Hasher_>>;

// An insert-only map implemented as a hash-table with open addressing (also
// called closed hashing). The table can contain up to 2^30 distinct keys. It is
// a 32-bit table independent of the process being 32-bit or 64-bit to ensure
// that tables can be read in processes of different bitness from those in which
// they were generated. This is a requirement of consumers of this data
// structure. In particular, the subresource_filter component in Weblayer
// generates tables in the 64-bit browser process but also accesses them the
// 32-bit renderer process.
//
// On normal operation load factor varies within range (1/4, 1/2]. The real load
// factor can be less or equal to 1/4 if rehashing is requested explicitly to
// allocate more memory for future insertions.
//
// The table discloses its internal structure in order to allow converting it to
// different formats. It consists of 2 vectors, |hash_table| and |entries|. The
// |entries| is a vector of pair<KeyType, ValueType>, whereas |hash_table|'s
// elements (also called slots) are indexes into the |entries| vector. If, for
// some i, hash_table[i] >= entries.size() then the i-th slot is empty. It is
// guaranteed that each of the entries is referenced by exactly one table slot.
//
// The Prober is a strategy class used to find a slot for a particular key by
// probing the key against a sequence of slots called a probe sequence. Often it
// stores one or more hashers - functors used to calculate hash values of the
// keys in order to parameterize the probe sequence. The Prober class should
// provide the following:
//
//  Public type(s):
//   KeyType - the type of the keys the table can store. Should be equal to the
//   ClosedHashTable's KeyType.
//
//  Public method(s):
//   template <typename SlotCompare>
//   uint32_t FindSlot(const KeyType& key,
//                     uint32_t table_size,
//                     const SlotCompare& compare) const;
//
//   Walks the probe sequence for the given |key| starting from some initial
//   slot calculated deterministically from the |key|, e.g. by computing its
//   hash code. The walk continues until it finds a hash table slot such that
//   compare(key, slot) returns true (which, for instance, can mean that the
//   slot is empty or its key is equal to |key|). Returns the index of that slot
//   in [0, table_size). The method is required to perform a finite number of
//   probes until it finds a slot.
//
// The same Prober class can be used to ensure compatibility when performing
// lookups in two different representations of what is conceptually the same
// hash map.
//
// TODO(pkalinnikov): Add key comparator.
template <typename KeyType_, typename ValueType_, typename Prober_>
class ClosedHashMap {
 public:
  using KeyType = KeyType_;
  using ValueType = ValueType_;
  using EntryType = std::pair<KeyType, ValueType>;
  using Prober = Prober_;

  static_assert(std::is_same<KeyType, typename Prober::KeyType>::value,
                "The KeyType should be the same in the Prober.");

  // Creates an empty hash-map with the default prober, and space allocated for
  // 4 distinct keys.
  ClosedHashMap() : ClosedHashMap(4) {}

  // Creates an empty hash-map with the specified |capacity|, so that this many
  // distinct keys can be inserted with no rehashing or reallocation taking
  // place. The |prober| is a strategy used for finding slots for the keys.
  explicit ClosedHashMap(uint32_t capacity, const Prober& prober = Prober())
      : hash_table_(CalculateHashTableSizeFor(capacity), EmptySlot()),
        prober_(prober) {
    entries_.reserve(capacity);
  }

  ClosedHashMap(const ClosedHashMap&) = delete;
  ClosedHashMap& operator=(const ClosedHashMap&) = delete;

  // Returns the number of distinct keys.
  uint32_t size() const { return entries_.size(); }

  // Returns the number of slots in the |hash_table|.
  uint32_t table_size() const { return hash_table_.size(); }

  const std::vector<uint32_t>& hash_table() const { return hash_table_; }
  const std::vector<EntryType>& entries() const { return entries_; }

  // Returns a pointer to the value stored for the given |key|, or nullptr if
  // the |key| is not present in the map. The returned pointer is guaranteed to
  // be valid as long as no new keys get added to the map (more strictly
  // speaking, as long as no reallocations happen for the |entries| vector).
  const ValueType* Get(const KeyType& key) const {
    const uint32_t entry_index = hash_table_[FindSlotForKey(key)];
    if (entry_index == EmptySlot())
      return nullptr;
    DCHECK_LT(entry_index, entries_.size());
    return &entries_[entry_index].second;
  }

  // Associates |value| with the given |key| if the |key| is not yet present in
  // the map, and returns true. Otherwise does nothing and returns false.
  bool Insert(const KeyType& key, ValueType value) {
    const uint32_t slot = FindSlotForKey(key);
    if (hash_table_[slot] != EmptySlot())
      return false;

    EmplaceKeyValue(slot, key, std::move(value));
    return true;
  }

  // Returns a reference to the value stored for the given |key|. If the |key|
  // is not present in the map, it's inserted and value-initialized. The same
  // guarantee on reference validity applies as for the result of Get(key).
  ValueType& operator[](const KeyType& key) {
    const uint32_t slot = FindSlotForKey(key);
    uint32_t entry_index = hash_table_[slot];
    if (entry_index == EmptySlot()) {
      entry_index = EmplaceKeyValue(slot, key, ValueType());
    }

    DCHECK_LT(entry_index, entries_.size());
    return entries_[entry_index].second;
  }

  // Resizes the |hash_table|, if necessary, so that at least |capacity|
  // distinct keys can be stored with the load factor being no higher than 1/2.
  void Rehash(uint32_t capacity) {
    if (capacity <= hash_table_.size() / 2)
      return;
    DCHECK_LE(static_cast<uint32_t>(entries_.size()), EmptySlot());

    hash_table_.assign(CalculateHashTableSizeFor(capacity), EmptySlot());
    for (uint32_t index = 0; index != entries_.size(); ++index) {
      const uint32_t slot = FindSlotForKey(entries_[index].first);
      DCHECK_EQ(hash_table_[slot], EmptySlot());
      hash_table_[slot] = index;
    }
  }

  // Reserves enough space so that |capacity| distinct keys can be stored with
  // the load factor being no higher than 1/2.
  void Reserve(uint32_t capacity) {
    Rehash(capacity);
    entries_.reserve(capacity);
  }

 private:
  // The indicator of absent entry for a certain slot in |hash_table|.
  static constexpr uint32_t EmptySlot() {
    return std::numeric_limits<uint32_t>::max();
  }

  // Returns the number of |hash_table| slots necessary to maintain a load
  // factor between 1/4 and 1/2 for the number of distinct keys given by
  // |capacity|.
  static uint32_t CalculateHashTableSizeFor(uint32_t capacity) {
    // TODO(pkalinnikov): Implement base::bits::Log2Ceiling for arbitrary types.
    const uint32_t capacity_32 = base::checked_cast<uint32_t>(capacity);
    const int power_of_two = base::bits::Log2Ceiling(capacity_32) + 1;
    CHECK_LT(power_of_two, std::numeric_limits<uint32_t>::digits);
    return static_cast<uint32_t>(1) << power_of_two;
  }

  // Adds a new |key|-|value| pair to the structure. Returns the index of the
  // newly created entry. The table is rehashed if the newly created entry
  // bursts the load factor above 1/2. Otherwise the new entry is associated
  // with a specific |slot| of the table.
  uint32_t EmplaceKeyValue(uint32_t slot, KeyType key, ValueType value) {
    const uint32_t entry_index = entries_.size();
    CHECK_LT(entry_index, EmptySlot());
    entries_.emplace_back(std::move(key), std::move(value));

    if (entry_index >= hash_table_.size() / 2) {
      Rehash(entry_index + 1);
    } else {
      DCHECK_LT(slot, hash_table_.size());
      DCHECK_EQ(hash_table_[slot], EmptySlot());
      hash_table_[slot] = entry_index;
    }

    return entry_index;
  }

  // Finds a slot such that it's either empty (indicating that the |key| is not
  // stored) or contains the |key|.
  uint32_t FindSlotForKey(const KeyType& key) const {
    return prober_.FindSlot(
        key, hash_table_.size(),
        [this](const KeyType& key, uint32_t slot_index) {
          DCHECK_LT(slot_index, hash_table_.size());
          const uint32_t entry_index = hash_table_[slot_index];
          DCHECK(entry_index == EmptySlot() || entry_index < entries_.size());
          return entry_index == EmptySlot() ||
                 entries_[entry_index].first == key;
        });
  }

  // Contains indices into |entries_|, or EmptySlot() for free table slots.
  // Always contains at least twice as many slots as there are elements in
  // |entries_|, i.e. the load factor is always less than or equal to 1/2.
  std::vector<uint32_t> hash_table_;

  // Contains all the inserted key-value pairs. The keys are unique. The size of
  // the vector is never greater than EmptySlot().
  std::vector<EntryType> entries_;

  // The strategy used to find a slot for a key.
  Prober prober_;
};

// The implementation of Prober that uses a simple form of quadratic probing.
// That is, for a given |key| the sequence of probes is the following (modulo
// the hash table size):
//   hash(key), hash(key) + 1, hash(key) + 3, ..., hash(key) + k * (k - 1) / 2
//
// To use this prober the hash table should maintain its size M equal to powers
// of two. It can be shown that in this case the aforementioned quadratic probe
// sequence for k <= M visits k distinct table slots.
//
// Template parameters:
//   KeyType_ - the type of the table's keys.
//   Hasher_ - the type of the functor used to calculate hashes of keys.
template <typename KeyType_, typename Hasher_>
class SimpleQuadraticProber {
 public:
  using KeyType = KeyType_;
  using Hasher = Hasher_;

  // Constructs the prober that uses the |hasher| functor to hash the keys.
  explicit SimpleQuadraticProber(const Hasher& hasher = Hasher())
      : hasher_(hasher) {}

  // Returns the slot index for the |key|. Requires that |compare| returns true
  // for at least one slot index between 0 and |table_size| - 1.
  template <typename SlotCompare>
  uint32_t FindSlot(const KeyType& key,
                    uint32_t table_size,
                    const SlotCompare& compare) const {
    DCHECK_GT(table_size, 0u);
    DCHECK_EQ(table_size & (table_size - 1), 0u);
    const uint32_t kMask = table_size - 1;

    uint32_t slot_index = hasher_(key) & kMask;
    // The loop will always be finite, since |compare| is guaranteed to return
    // true for at least 1 slot index, and the probe sequence visits all slots.
    for (uint32_t step_size = 1; !compare(key, slot_index); ++step_size) {
      DCHECK_LT(step_size, table_size);
      slot_index = (slot_index + step_size) & kMask;
    }

    DCHECK_LT(slot_index, table_size);
    return slot_index;
  }

 private:
  // The functor used to calculate hashes of keys.
  Hasher hasher_;
};

}  // namespace url_pattern_index

#endif  // COMPONENTS_URL_PATTERN_INDEX_CLOSED_HASH_MAP_H_
