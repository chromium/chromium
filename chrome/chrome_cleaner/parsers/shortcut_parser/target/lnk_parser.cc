// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/shortcut_parser/target/lnk_parser.h"

#include <windows.h>

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"

namespace chrome_cleaner {

namespace {

const size_t kShortSize = sizeof(SHORT);
const size_t kUInt16Size = sizeof(uint16_t);

struct UTF16Offsets {
  DWORD prefix_offset;
  DWORD suffix_offset;
};
const int kUTF16OffsetsSize = sizeof(UTF16Offsets);

// File size of 1 MB. It's very unlikely that a LNK file will be larger than
// that.
const int64_t kMaximumFileSize = 1 * 1024 * 1024;

// Retrieves a 2 byte unsigned short from the provided |buffer| and also
// modifies the value of |current_byte| to point to the next byte after the
// short.
bool ReadUnsignedShort(const std::vector<BYTE>& buffer,
                       DWORD* current_byte,
                       uint16_t* result) {
  if (*current_byte + kUInt16Size >= buffer.size()) {
    return false;
  }
  memcpy_s(result, kUInt16Size, buffer.data() + *current_byte, kUInt16Size);

  *current_byte += kUInt16Size;
  return true;
}

bool NullTerminatedASCIIBufferToString16(const std::vector<BYTE>& buffer,
                                         DWORD* current_byte,
                                         base::string16* parsed_string) {
  const DWORD string_start = *current_byte;
  const int kMaxCharactersToRead = buffer.size() - *current_byte;
  int string_size =
      strnlen_s(reinterpret_cast<const char*>(buffer.data() + string_start),
                kMaxCharactersToRead);

  // If the null character was not found, strnlen_s will return the
  // value of kMaxCharactersToRead. This is an indicator of a bad format, since
  // the strings we read will never be at the end of the buffer.
  if (string_size == kMaxCharactersToRead)
    return false;

  // Consider the null terminated byte at the end of the string.
  *current_byte += string_size + 1;
  const char* string_ptr =
      reinterpret_cast<const char*>(buffer.data() + string_start);
  base::UTF8ToWide(string_ptr, string_size, parsed_string);
  return true;
}

bool NullTerminatedUtf16BufferToString16(const std::vector<BYTE>& buffer,
                                         DWORD* current_byte,
                                         base::string16* parsed_string) {
  const DWORD string_start = *current_byte;
  const int kMaxWideCharactersToRead =
      (buffer.size() - *current_byte) / sizeof(wchar_t);
  int string_size =
      wcsnlen_s(reinterpret_cast<const wchar_t*>(buffer.data() + string_start),
                kMaxWideCharactersToRead);

  // If the null character was not found, strnlen_s will return the
  // value of kMaxCharactersToRead. This is an indicator of a bad format, since
  // the strings we read will never be at the end of the buffer.
  if (string_size == kMaxWideCharactersToRead)
    return false;

  // Consider the null terminated byte at the end of the string.
  *current_byte += (string_size + 1) * sizeof(wchar_t);
  const wchar_t* string_ptr =
      reinterpret_cast<const wchar_t*>(buffer.data() + string_start);
  base::WideToUTF16(string_ptr, string_size, parsed_string);
  return true;
}

// Retrieves a null terminated string from the provided |buffer| and also
// modifies the value of |current_byte| to point to the next byte after the
// string.
bool NullTerminatedStringToString16(const std::vector<BYTE>& buffer,
                                    bool is_unicode,
                                    DWORD* current_byte,
                                    base::string16* parsed_string) {
  if (*current_byte >= buffer.size()) {
    LOG(ERROR) << "Error parsing null terminated string";
    return false;
  }

  return (is_unicode) ? NullTerminatedUtf16BufferToString16(
                            buffer, current_byte, parsed_string)
                      : NullTerminatedASCIIBufferToString16(
                            buffer, current_byte, parsed_string);
}

// Reads the size of a string structure and then moves the value of
// current_byte to the end of it.
bool SkipUtf16StringStructure(const std::vector<BYTE>& buffer,
                              DWORD* current_byte) {
  uint16_t structure_size;
  if (!ReadUnsignedShort(buffer, current_byte, &structure_size)) {
    LOG(ERROR) << "Error reading string structure size";
    return false;
  }

  // The structure size is the number of characters present on the
  // encoded string, therefore for each character we need to skip
  // kShortSize bytes.
  *current_byte += structure_size * kShortSize;

  if (*current_byte >= buffer.size()) {
    return false;
  }
  return true;
}

// Reads the size of a string structure, stores its contents in |parsed_string|
// and then moves the value of current_byte to the end of it.
bool ReadUtf16StringStructure(const std::vector<BYTE>& buffer,
                              DWORD* current_byte,
                              base::string16* parsed_string) {
  uint16_t string_size;
  if (!ReadUnsignedShort(buffer, current_byte, &string_size)) {
    LOG(ERROR) << "Error reading string structure";
    return false;
  }

  if (string_size == 0) {
    LOG(ERROR) << "Error reading string structure: zero length.";
    return false;
  }

  if (*current_byte + string_size * sizeof(wchar_t) > buffer.size()) {
    LOG(ERROR) << "Error: string structure size: " << string_size
               << " is longer than rest of file";
    return false;
  }

  const wchar_t* string_ptr =
      reinterpret_cast<const wchar_t*>(buffer.data() + *current_byte);
  base::WideToUTF16(string_ptr, string_size, parsed_string);
  *current_byte += string_size * sizeof(wchar_t);
  return true;
}

}  // namespace

namespace internal {

const LnkHeader* ParseLnkHeader(std::vector<BYTE>* file_buffer) {
  if (file_buffer->size() < kHeaderSize)
    return nullptr;

  return reinterpret_cast<const LnkHeader*>(file_buffer->data());
}

LnkInfoPartialHeader* LocateAndParseLnkInfoPartialHeader(
    std::vector<BYTE>* file_buffer,
    DWORD* output_offset) {
  const LnkHeader* lnk_header = ParseLnkHeader(file_buffer);
  if (!lnk_header)
    return nullptr;
  DWORD structure_offset = kHeaderSize;

  // If there is a target id list skip it.
  constexpr BYTE kLinkTargetIdListPresentFlag = 0x01;
  if (lnk_header->lnk_flags & kLinkTargetIdListPresentFlag) {
    uint16_t id_list_size;
    if (!ReadUnsignedShort(*file_buffer, &structure_offset, &id_list_size)) {
      LOG(ERROR) << "Error reading id list size";
      return nullptr;
    }

    structure_offset += id_list_size;
    if (structure_offset >= file_buffer->size()) {
      return nullptr;
    }
  }

  // The path is stored on the link info structure
  if (structure_offset + internal::kLnkInfoPartialHeaderSize >=
      file_buffer->size()) {
    return nullptr;
  }
  if (output_offset)
    *output_offset = structure_offset;

  return reinterpret_cast<internal::LnkInfoPartialHeader*>(file_buffer->data() +
                                                           structure_offset);
}

}  // namespace internal

mojom::LnkParsingResult internal::ParseLnkBytes(
    std::vector<BYTE> file_buffer,
    ParsedLnkFile* parsed_shortcut) {
  DCHECK(parsed_shortcut);
  // This variable is used to keep track which part of the buffer we are
  // currently parsing, please note that its value will be modified in all
  // of the function it is passed as parameter.
  DWORD current_byte = 0;

  // Start of the lnk file parsing.
  const internal::LnkHeader* lnk_file_header =
      internal::ParseLnkHeader(&file_buffer);
  if (!lnk_file_header)
    return mojom::LnkParsingResult::BAD_FORMAT;

  // The next word corresponds to the lnk file flags, however we are only
  // interested on the flags stored on the first byte.
  bool has_arguments = lnk_file_header->lnk_flags & 0x20;
  bool has_working_dir = lnk_file_header->lnk_flags & 0x10;
  bool has_relative_path = lnk_file_header->lnk_flags & 0x08;
  bool has_name = lnk_file_header->lnk_flags & 0x04;
  bool has_link_info = lnk_file_header->lnk_flags & 0x02;
  bool has_icon_location = lnk_file_header->lnk_flags & 0x40;

  // The target path is stored on the link_info structure, if that is not
  // present we cannot recover the file path.
  if (!has_link_info) {
    LOG(ERROR) << "The file has no link info structure present";
    return mojom::LnkParsingResult::NO_LINK_INFO_STRUCTURE;
  }

  const internal::LnkInfoPartialHeader* partial_header =
      internal::LocateAndParseLnkInfoPartialHeader(&file_buffer, &current_byte);
  if (!partial_header)
    return mojom::LnkParsingResult::BAD_FORMAT;
  const DWORD structure_beginning = current_byte;

  // The path is stored on the link info structure
  if (current_byte + internal::kLnkInfoPartialHeaderSize >=
      file_buffer.size()) {
    return mojom::LnkParsingResult::BAD_FORMAT;
  }

  // The flag value 0x01 means that the shortcut points to a local file. If
  // it's not set, the shortcut points to a network location (0x02). It's
  // possible for both the local and network flags to be set, if the shortcut
  // target is inside a local folder that has Windows file sharing turned on.
  constexpr DWORD kLocalFileFlag = 0x01UL;
  if ((partial_header->flags & kLocalFileFlag) != kLocalFileFlag) {
    LOG(ERROR) << "LNK file corresponding to a network location";
    return mojom::LnkParsingResult::LNK_TO_NETWORK_LOCATION;
  }

  // Point to the offsets.
  current_byte += internal::kLnkInfoPartialHeaderSize;

  // According Microsoft's documentation, if the header size is greater
  // or equal to 0x00000024 then we have unicode strings.
  const DWORD kUnicodeHeaderSize = 0x00000024;
  bool is_unicode = partial_header->header_size >= kUnicodeHeaderSize;

  DWORD path_prefix_offset;
  DWORD path_suffix_offset;

  if (is_unicode) {
    if (current_byte + kUTF16OffsetsSize >= file_buffer.size()) {
      return mojom::LnkParsingResult::BAD_FORMAT;
    }

    UTF16Offsets utf16_offsets;
    memcpy_s(&utf16_offsets, kUTF16OffsetsSize,
             file_buffer.data() + current_byte, kUTF16OffsetsSize);
    path_prefix_offset = utf16_offsets.prefix_offset;
    path_suffix_offset = utf16_offsets.suffix_offset;
  } else {
    path_prefix_offset = partial_header->ascii_prefix_offset;
    path_suffix_offset = partial_header->ascii_suffix_offset;
  }

  current_byte = structure_beginning + path_prefix_offset;

  base::string16 prefix_string;
  if (!NullTerminatedStringToString16(file_buffer, is_unicode, &current_byte,
                                      &prefix_string)) {
    LOG(ERROR) << "Error parsing path prefix";
    return mojom::LnkParsingResult::BAD_FORMAT;
  }

  // Recover the path by appending the suffix to the prefix.
  current_byte = structure_beginning + path_suffix_offset;
  base::string16 suffix_string;

  if (!NullTerminatedStringToString16(file_buffer, is_unicode, &current_byte,
                                      &suffix_string)) {
    LOG(ERROR) << "Error parsing path suffix";
    return mojom::LnkParsingResult::BAD_FORMAT;
  }

  parsed_shortcut->target_path = prefix_string + suffix_string;

  // Skip the rest of the structure.
  current_byte = structure_beginning + partial_header->structure_size;

  // Skip the rest of the String structures that are present.
  if (has_name && !SkipUtf16StringStructure(file_buffer, &current_byte)) {
    return mojom::LnkParsingResult::BAD_FORMAT;
  }

  if (has_relative_path &&
      !SkipUtf16StringStructure(file_buffer, &current_byte)) {
    return mojom::LnkParsingResult::BAD_FORMAT;
  }

  if (has_working_dir &&
      !SkipUtf16StringStructure(file_buffer, &current_byte)) {
    return mojom::LnkParsingResult::BAD_FORMAT;
  }

  // Retrieve the arguments.
  if (has_arguments &&
      !ReadUtf16StringStructure(file_buffer, &current_byte,
                                &parsed_shortcut->command_line_arguments)) {
    LOG(ERROR) << "Error reading argument list";
    return mojom::LnkParsingResult::BAD_FORMAT;
  }

  // Retrieve the icon location.
  if (has_icon_location &&
      !ReadUtf16StringStructure(file_buffer, &current_byte,
                                &parsed_shortcut->icon_location)) {
    LOG(ERROR) << "Error reading icon location";
    return mojom::LnkParsingResult::BAD_FORMAT;
  }

  return mojom::LnkParsingResult::SUCCESS;
}

// Please note that the documentation used to write this parser was obtained
// from the following link:
// https://msdn.microsoft.com/en-us/library/dd871305.aspx
mojom::LnkParsingResult ParseLnk(base::win::ScopedHandle file_handle,
                                 ParsedLnkFile* parsed_shortcut) {
  if (!file_handle.IsValid()) {
    LOG(ERROR) << "Invalid File Handle";
    return mojom::LnkParsingResult::INVALID_HANDLE;
  }

  base::File lnk_file(file_handle.Take());
  int64_t lnk_file_size = lnk_file.GetLength();
  if (lnk_file_size <= 0) {
    LOG(ERROR) << "Error getting file size";
    return mojom::LnkParsingResult::INVALID_LNK_FILE_SIZE;
  } else if (lnk_file_size >= kMaximumFileSize) {
    LOG(ERROR) << "Unexpectedly large file size: " << lnk_file_size;
    return mojom::LnkParsingResult::INVALID_LNK_FILE_SIZE;
  }

  std::vector<BYTE> file_buffer(lnk_file_size);
  int bytes_read = lnk_file.Read(
      /*offset=*/0, reinterpret_cast<char*>(file_buffer.data()), lnk_file_size);
  if (bytes_read == -1) {
    LOG(ERROR) << "Error reading lnk file";
    return mojom::LnkParsingResult::READING_ERROR;
  }

  if (bytes_read != lnk_file_size) {
    LOG(ERROR) << "read less bytes than the actual file size, bytes read: "
               << bytes_read << " file size: " << lnk_file_size;
    return mojom::LnkParsingResult::READING_ERROR;
  }

  return internal::ParseLnkBytes(file_buffer, parsed_shortcut);
}

}  // namespace chrome_cleaner
