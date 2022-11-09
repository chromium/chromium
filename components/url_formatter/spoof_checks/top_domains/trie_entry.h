// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TRIE_ENTRY_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TRIE_ENTRY_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "net/tools/huffman_trie/huffman/huffman_builder.h"
#include "net/tools/huffman_trie/trie_entry.h"

namespace url_formatter {

// The |SkeletonType| and |TopDomainEntry| are mirrored in trie_entry.h. These
// are used to insert and read nodes from the Trie.
// The type of skeleton in the trie node. This type is encoded by 2 bits in the
// trie.
enum SkeletonType {
  // The skeleton represents the full domain (e.g. google.corn).
  kFull = 0,
  // The skeleton represents the domain with '.'s and '-'s removed (e.g.
  // googlecorn).
  kSeparatorsRemoved = 1,
  // Max value used to determine the number of different types. Update this and
  // |kSkeletonTypeBitLength| when new SkeletonTypes are added.
  kMaxValue = kSeparatorsRemoved
};

const uint8_t kSkeletonTypeBitLength = 1;

namespace top_domains {

struct TopDomainEntry {
  std::string skeleton;
  std::string top_domain;
  bool is_top_500;
  SkeletonType skeleton_type;
};

class TopDomainTrieEntry : public net::huffman_trie::TrieEntry {
 public:
  explicit TopDomainTrieEntry(
      const net::huffman_trie::HuffmanRepresentationTable& huffman_table,
      net::huffman_trie::HuffmanBuilder* huffman_builder,
      TopDomainEntry* entry);
  ~TopDomainTrieEntry() override;

  // huffman_trie::TrieEntry:
  std::string name() const override;
  bool WriteEntry(net::huffman_trie::TrieBitBuffer* writer) const override;

 private:
  const raw_ref<const net::huffman_trie::HuffmanRepresentationTable>
      huffman_table_;
  raw_ptr<net::huffman_trie::HuffmanBuilder> huffman_builder_;
  raw_ptr<TopDomainEntry> entry_;
};

using TopDomainEntries = std::vector<std::unique_ptr<TopDomainEntry>>;

}  // namespace top_domains

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TRIE_ENTRY_H_
