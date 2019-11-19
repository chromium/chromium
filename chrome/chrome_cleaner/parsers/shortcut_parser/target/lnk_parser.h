// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_TARGET_LNK_PARSER_H_
#define CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_TARGET_LNK_PARSER_H_

#include <windows.h>
#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"

namespace chrome_cleaner {

struct ParsedLnkFile;

namespace internal {

// Auxiliary structures for the shortcut parsing.
struct LnkHeader {
  DWORD header_size;
  DWORD signature[4];
  DWORD lnk_flags;
  DWORD file_attributes;
  DWORDLONG creation_time;
  DWORDLONG access_time;
  DWORDLONG write_time;
  DWORD file_size;
  DWORD icon_index;
  DWORD show_command;
  SHORT hot_key;
  SHORT reserved_1;
  DWORD reserved_2;
  DWORD reserved_3;
} __attribute__((packed));
const int64_t kHeaderSize = 0x0000004C;
static_assert(
    sizeof(internal::LnkHeader) == internal::kHeaderSize,
    "Size of LnkHeader structure is different than internal::kHeaderSize");

// Since the link_info structure may contain optional values, we hold the first
// set of values that we know will always be present.
struct LnkInfoPartialHeader {
  DWORD structure_size;
  DWORD header_size;
  DWORD flags;
  DWORD volume_id_offset;
  DWORD ascii_prefix_offset;
  DWORD network_offset;
  DWORD ascii_suffix_offset;
};
const int kLnkInfoPartialHeaderSize = sizeof(LnkInfoPartialHeader);

// Warning: Please note that this will only be valid during the life of
// |file_buffer|.
const LnkHeader* ParseLnkHeader(std::vector<BYTE>* file_buffer);

// Warning: This is only valid during the lifetime of |file_buffer|.
LnkInfoPartialHeader* LocateAndParseLnkInfoPartialHeader(
    std::vector<BYTE>* file_buffer,
    DWORD* output_offset);

mojom::LnkParsingResult ParseLnkBytes(std::vector<BYTE> file_buffer,
                                      ParsedLnkFile* parsed_shortcut);

}  // namespace internal

struct ParsedLnkFile {
  base::string16 target_path;
  base::string16 command_line_arguments;
  base::string16 icon_location;
};

mojom::LnkParsingResult ParseLnk(base::win::ScopedHandle file_handle,
                                 ParsedLnkFile* result);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_TARGET_LNK_PARSER_H_
