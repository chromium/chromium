// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/top_domains/top_domain_state_generator.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/tools/huffman_trie/huffman/huffman_builder.h"
#include "net/tools/huffman_trie/trie/trie_bit_buffer.h"
#include "net/tools/huffman_trie/trie/trie_writer.h"

using net::huffman_trie::HuffmanBuilder;
using net::huffman_trie::HuffmanRepresentationTable;
using net::huffman_trie::TrieWriter;

namespace url_formatter {

namespace top_domains {

namespace {

static const char kNewLine[] = "\n";
static const char kIndent[] = "  ";

// Replaces the first occurrence of "[[" + name + "]]" in |*tpl| with
// |value|.
bool ReplaceTag(const std::string& name,
                const std::string& value,
                std::string* tpl) {
  std::string tag = "[[" + name + "]]";

  size_t start_pos = tpl->find(tag);
  if (start_pos == std::string::npos) {
    return false;
  }

  tpl->replace(start_pos, tag.length(), value);
  return true;
}

// Formats the bytes in |bytes| as an C++ array initializer and returns the
// resulting string.
std::string FormatVectorAsArray(const std::vector<uint8_t>& bytes) {
  std::string output = "{";
  output.append(kNewLine);
  output.append(kIndent);
  output.append(kIndent);

  size_t bytes_on_current_line = 0;

  for (size_t i = 0; i < bytes.size(); ++i) {
    base::StringAppendF(&output, "0x%02x,", bytes[i]);

    bytes_on_current_line++;
    if (bytes_on_current_line >= 12 && (i + 1) < bytes.size()) {
      output.append(kNewLine);
      output.append(kIndent);
      output.append(kIndent);

      bytes_on_current_line = 0;
    } else if ((i + 1) < bytes.size()) {
      output.append(" ");
    }
  }

  output.append(kNewLine);
  output.append("}");

  return output;
}

HuffmanRepresentationTable ApproximateHuffman(const TopDomainEntries& entries) {
  HuffmanBuilder huffman_builder;
  for (const auto& entry : entries) {
    for (const auto& c : entry->skeleton) {
      huffman_builder.RecordUsage(c);
    }
    for (const auto& c : entry->top_domain) {
      huffman_builder.RecordUsage(c);
    }
    huffman_builder.RecordUsage(net::huffman_trie::kTerminalValue);
    huffman_builder.RecordUsage(net::huffman_trie::kEndOfTableValue);
  }

  return huffman_builder.ToTable();
}

}  // namespace

TopDomainStateGenerator::TopDomainStateGenerator() = default;

TopDomainStateGenerator::~TopDomainStateGenerator() = default;

std::string TopDomainStateGenerator::Generate(
    const std::string& preload_template,
    const TopDomainEntries& entries) {
  std::string output = preload_template;

  // The trie generation process for the whole data is run twice, the first time
  // using an approximate Huffman table. During this first run, the correct
  // character frequencies are collected which are then used to calculate the
  // most space efficient Huffman table for the given inputs. This table is used
  // for the second run.

  HuffmanRepresentationTable approximate_table = ApproximateHuffman(entries);
  HuffmanBuilder huffman_builder;

  // Create trie entries for the first pass.
  std::vector<std::unique_ptr<TopDomainTrieEntry>> trie_entries;
  std::vector<net::huffman_trie::TrieEntry*> raw_trie_entries;
  for (const auto& entry : entries) {
    auto trie_entry = std::make_unique<TopDomainTrieEntry>(
        approximate_table, &huffman_builder, entry.get());
    raw_trie_entries.push_back(trie_entry.get());
    trie_entries.push_back(std::move(trie_entry));
  }

  TrieWriter writer(approximate_table, &huffman_builder);
  uint32_t root_position;
  if (!writer.WriteEntries(raw_trie_entries, &root_position))
    return std::string();

  HuffmanRepresentationTable optimal_table = huffman_builder.ToTable();
  TrieWriter new_writer(optimal_table, &huffman_builder);

  // Create trie entries using the optimal table for the second pass.
  raw_trie_entries.clear();
  trie_entries.clear();
  for (const auto& entry : entries) {
    auto trie_entry = std::make_unique<TopDomainTrieEntry>(
        optimal_table, &huffman_builder, entry.get());
    raw_trie_entries.push_back(trie_entry.get());
    trie_entries.push_back(std::move(trie_entry));
  }

  if (!new_writer.WriteEntries(raw_trie_entries, &root_position))
    return std::string();

  uint32_t new_length = new_writer.position();
  std::vector<uint8_t> huffman_tree = huffman_builder.ToVector();
  new_writer.Flush();

  ReplaceTag("HUFFMAN_TREE", FormatVectorAsArray(huffman_tree), &output);

  ReplaceTag("TOP_DOMAINS_TRIE", FormatVectorAsArray(new_writer.bytes()),
             &output);

  ReplaceTag("TOP_DOMAINS_TRIE_BITS", base::NumberToString(new_length),
             &output);
  ReplaceTag("TOP_DOMAINS_TRIE_ROOT", base::NumberToString(root_position),
             &output);

  return output;
}

}  // namespace top_domains

}  // namespace url_formatter
