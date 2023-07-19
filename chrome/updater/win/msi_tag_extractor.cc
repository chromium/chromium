// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/msi_tag_extractor.h"

#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

namespace {

// A fixed string serves as the magic signature to identify whether it is the
// start of a valid tag.
constexpr char kTagMagicSignature[] = "Gact2.0Omaha";

constexpr size_t kMagicSignatureLength = std::size(kTagMagicSignature) - 1;
constexpr char kStringMapDelimeter = '&';
constexpr char kKeyValueDelimeter = '=';

constexpr int kMaxTagStringLengthAllowed = 81920;  // 80K

// The smallest meaningful tag is 'Gact2.0Omaha\0\0', indicating a zero-length
// payload.
constexpr int kMinTagLengthAllowed =
    static_cast<int>(kMagicSignatureLength + sizeof(char) * 2);

constexpr int kMaxTagLength = kMaxTagStringLengthAllowed + kMinTagLengthAllowed;

// Read a `uint16_t` value from a big-endian `buffer`.
uint16_t GetValueFromBuffer(const char* buffer) {
  CHECK(buffer);

  uint16_t value = 0;
  for (size_t i = 0; i < sizeof(uint16_t); ++i) {
    value <<= 8;
    value |= static_cast<uint8_t>(buffer[i]);
  }
  return value;
}

bool IsValidTagKey(const std::string& str) {
  return base::ranges::find_if(str, [](char c) {
           return !base::IsAsciiAlphaNumeric(c);
         }) == str.end();
}

bool IsValidTagValue(const std::string& str) {
  return base::ranges::find_if(str, [](char c) {
           constexpr char kValidChars[] = "{}[]-% _";
           return base::IsAsciiAlphaNumeric(c) ? false
                                               : !strchr(kValidChars, c);
         }) == str.end();
}

// Loads the last 80K bytes from `filename`, and searches for 'Gact2.0Omaha'. If
// found, returns the tag buffer.
std::vector<char> ReadTagToBuffer(const base::FilePath& filename) {
  base::File file(filename, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return {};
  }

  const int64_t file_length = file.GetLength();
  if (file_length < kMinTagLengthAllowed) {
    return {};
  }

  // Read at most the last `kMaxTagLength` bytes from file.
  int bytes_to_read = kMaxTagLength;
  if (file_length > kMaxTagLength) {
    if (file.Seek(base::File::FROM_END, -kMaxTagLength) != kMaxTagLength) {
      return {};
    }
  } else {
    bytes_to_read = file_length;
  }

  std::vector<char> buffer(bytes_to_read + 1);
  const int num_bytes_read = file.ReadAtCurrentPos(&buffer[0], bytes_to_read);
  if (num_bytes_read != bytes_to_read) {
    return {};
  }

  // Search for the last occurrence of the magic signature in the loaded buffer.
  const auto magic_signature_position =
      base::ranges::find_end(buffer, kTagMagicSignature);
  return std::distance(magic_signature_position, buffer.end()) >=
                 kMinTagLengthAllowed
             ? std::vector(magic_signature_position, buffer.end())
             : std::vector<char>();
}

bool ParseMagicSignature(const std::vector<char>& tag_buffer,
                         size_t& parse_position) {
  if (tag_buffer.size() <= parse_position + kMagicSignatureLength) {
    return false;
  }

  const bool result = memcmp(&tag_buffer[parse_position], kTagMagicSignature,
                             kMagicSignatureLength) == 0;
  parse_position += kMagicSignatureLength;
  return result;
}

uint16_t ParseTagLength(const std::vector<char>& tag_buffer,
                        size_t& parse_position) {
  const uint16_t tag_length = GetValueFromBuffer(&tag_buffer[parse_position]);
  parse_position += sizeof(tag_length);
  return tag_length;
}

absl::optional<std::pair<std::string, std::string>> ParseKeyValueSubstring(
    const std::string& key_value_str) {
  const size_t found = key_value_str.find(kKeyValueDelimeter);
  if (found == std::string::npos) {
    return {};
  }
  const std::string key = key_value_str.substr(0, found);
  const std::string value = key_value_str.substr(found + 1);

  return (!key.empty() && IsValidTagKey(key) && IsValidTagValue(value))
             ? absl::optional<std::pair<std::string, std::string>>(
                   std::make_pair(key, value))
             : absl::nullopt;
}

// Parses a string in the format: "key1=value1&key2=value2&....keyN=valueN". All
// keys/values must be alphanumeric strings. Values can be empty. Unrecognized
// tags will be ignored.
base::flat_map<std::string, std::string> ParseSimpleAsciiStringMap(
    const std::vector<char>& tag_buffer,
    size_t parse_position,
    size_t tag_length) {
  // Construct a string with at most `tag_length` characters, but stop at the
  // first '\0' if it comes before that.
  const std::string tag_string(
      &tag_buffer[parse_position],
      strnlen(&tag_buffer[parse_position], tag_length));

  base::flat_map<std::string, std::string> tag_map;
  size_t start_pos = 0;
  size_t found = std::string::npos;
  do {
    found = tag_string.find_first_of(kStringMapDelimeter, start_pos);
    auto keyvalue =
        ParseKeyValueSubstring(tag_string.substr(start_pos, found - start_pos));
    if (keyvalue) {
      tag_map.insert(*keyvalue);
    }
    start_pos = found + 1;
  } while (found != std::string::npos);

  return tag_map;
}

base::flat_map<std::string, std::string> ParseTagBuffer(
    const std::vector<char>& tag_buffer) {
  if (tag_buffer.empty()) {
    return {};
  }
  size_t parse_position = 0;
  if (!ParseMagicSignature(tag_buffer, parse_position)) {
    return {};
  }

  const uint16_t tag_length = ParseTagLength(tag_buffer, parse_position);

  // Tag length should not exceed remaining buffer size.
  if (tag_length > tag_buffer.size() - parse_position) {
    return {};
  }

  return ParseSimpleAsciiStringMap(tag_buffer, parse_position, tag_length);
}

}  // namespace

base::flat_map<std::string, std::string> ExtractTagMap(
    const base::FilePath& filename) {
  return ParseTagBuffer(ReadTagToBuffer(filename));
}

}  // namespace updater
