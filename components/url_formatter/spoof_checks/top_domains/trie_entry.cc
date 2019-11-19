// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/top_domains/trie_entry.h"
#include "base/strings/string_util.h"
#include "net/tools/huffman_trie/trie/trie_bit_buffer.h"
#include "net/tools/huffman_trie/trie/trie_writer.h"

namespace url_formatter {

namespace top_domains {

TopDomainTrieEntry::TopDomainTrieEntry(
    const net::huffman_trie::HuffmanRepresentationTable& huffman_table,
    net::huffman_trie::HuffmanBuilder* huffman_builder,
    TopDomainEntry* entry)
    : huffman_table_(huffman_table),
      huffman_builder_(huffman_builder),
      entry_(entry) {}

TopDomainTrieEntry::~TopDomainTrieEntry() {}

std::string TopDomainTrieEntry::name() const {
  return entry_->skeleton;
}

bool TopDomainTrieEntry::WriteEntry(
    net::huffman_trie::TrieBitBuffer* writer) const {
  if (entry_->skeleton == entry_->top_domain) {
    writer->WriteBit(1);
    writer->WriteBit(entry_->is_top_500 ? 1 : 0);
    return true;
  }
  writer->WriteBit(0);
  writer->WriteBit(entry_->is_top_500 ? 1 : 0);

  std::string top_domain = entry_->top_domain;
  // With the current top 10,000 domains, this optimization reduces the
  // additional binary size required for the trie from 71 kB to 59 kB.
  if (base::EndsWith(top_domain, ".com",
                     base::CompareCase::INSENSITIVE_ASCII)) {
    writer->WriteBit(1);
    top_domain = top_domain.substr(0, top_domain.size() - 4);
  } else {
    writer->WriteBit(0);
  }

  for (const auto& c : top_domain) {
    writer->WriteChar(c, huffman_table_, huffman_builder_);
  }
  writer->WriteChar(net::huffman_trie::kEndOfTableValue, huffman_table_,
                    huffman_builder_);
  return true;
}

}  // namespace top_domains

}  // namespace url_formatter
