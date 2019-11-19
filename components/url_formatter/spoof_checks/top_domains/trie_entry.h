// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TRIE_ENTRY_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TRIE_ENTRY_H_

#include <string>
#include <vector>

#include "net/tools/huffman_trie/huffman/huffman_builder.h"
#include "net/tools/huffman_trie/trie_entry.h"

namespace url_formatter {

namespace top_domains {

struct TopDomainEntry {
  std::string skeleton;
  std::string top_domain;
  bool is_top_500;
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
  const net::huffman_trie::HuffmanRepresentationTable& huffman_table_;
  net::huffman_trie::HuffmanBuilder* huffman_builder_;
  TopDomainEntry* entry_;
};

using TopDomainEntries = std::vector<std::unique_ptr<TopDomainEntry>>;

}  // namespace top_domains

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_TOP_DOMAINS_TRIE_ENTRY_H_
