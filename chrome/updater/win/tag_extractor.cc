// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/tag_extractor.h"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "base/numerics/checked_math.h"
#include "base/win/pe_image_reader.h"
#include "build/build_config.h"
#include "chrome/updater/win/tag_extractor_impl.h"

namespace updater {

namespace {

// Magic strings used to identify the tag in the binary.
constexpr uint8_t kTagStartMagicUtf8[] = {'G', 'a', 'c', 't', '2', '.',
                                          '0', 'O', 'm', 'a', 'h', 'a'};
constexpr uint8_t kTagStartMagicUtf16[] = {0, 'G', 0, 'a', 0, 'c', 0, 't',
                                           0, '2', 0, '.', 0, '0', 0, 'O',
                                           0, 'm', 0, 'a', 0, 'h', 0, 'a'};
constexpr uint8_t kTagEndMagicUtf16[] = {0, 'a', 0, 'h', 0, 'a', 0, 'm',
                                         0, 'O', 0, '0', 0, '.', 0, '2',
                                         0, 't', 0, 'c', 0, 'a', 0, 'G'};

// Converts a big-endian 2-byte value to little-endian and returns it
// as a uint16_t.
uint16_t BigEndianReadU16(BinaryConstIt it) {
  static_assert(ARCH_CPU_LITTLE_ENDIAN, "Machine should be little-endian.");
  return (uint16_t{*it} << 8) + (uint16_t{*(it + 1)});
}

std::string ReadTagUtf8(BinaryConstIt cert_begin, BinaryConstIt cert_end) {
  const uint8_t* magic_begin = std::begin(kTagStartMagicUtf8);
  const uint8_t* magic_end = std::end(kTagStartMagicUtf8);

  BinaryConstIt magic_str =
      std::search(cert_begin, cert_end, magic_begin, magic_end);
  if (magic_str == cert_end)
    return std::string();

  BinaryConstIt taglen_buf =
      AdvanceIt(magic_str, magic_end - magic_begin, cert_end);

  // Checks that the stored tag length is found within the binary.
  if (!CheckRange(taglen_buf, sizeof(uint16_t), cert_end))
    return std::string();

  // Tag length is stored as a big-endian uint16_t.
  const uint16_t tag_len = BigEndianReadU16(taglen_buf);

  BinaryConstIt tag_buf = AdvanceIt(taglen_buf, sizeof(uint16_t), cert_end);
  if (tag_buf == cert_end)
    return std::string();

  // Checks that the specified tag is found within the binary.
  if (!CheckRange(tag_buf, tag_len, cert_end))
    return std::string();

  return std::string(tag_buf, tag_buf + tag_len);
}

std::string ReadTagUtf16(BinaryConstIt cert_begin, BinaryConstIt cert_end) {
  const uint8_t* magic_begin = std::begin(kTagStartMagicUtf16);
  const uint8_t* magic_end = std::end(kTagStartMagicUtf16);

  BinaryConstIt magic_str =
      std::search(cert_begin, cert_end, magic_begin, magic_end);
  if (magic_str == cert_end)
    return std::string();

  BinaryConstIt tag_buf =
      AdvanceIt(magic_str, magic_end - magic_begin, cert_end);

  BinaryConstIt tag_buf_end =
      std::search(tag_buf, cert_end, std::begin(kTagEndMagicUtf16),
                  std::end(kTagEndMagicUtf16));
  if (tag_buf_end == cert_end)
    return std::string();

  // UTF-16 strings can only have an even number of bytes since each
  // character occupies two bytes.
  if ((tag_buf_end - tag_buf) % 2 != 0)
    return std::string();

  std::wstring tag_utf16;
  tag_utf16.resize((tag_buf_end - tag_buf) / sizeof(uint16_t));

  // Converts the UTF-16 tag from big-endian to little-endian.
  size_t tag_utf16_idx = 0;
  for (auto it = tag_buf; it < tag_buf_end; it += sizeof(uint16_t)) {
    tag_utf16[tag_utf16_idx] = std::wstring::value_type{BigEndianReadU16(it)};
    ++tag_utf16_idx;
  }

  // Converts the tag from UTF-16 to UTF-8.
  const int str_size =
      ::WideCharToMultiByte(CP_UTF8, 0, &tag_utf16[0], tag_utf16.size(),
                            nullptr, 0, nullptr, nullptr);
  std::string tag;
  tag.resize(str_size);
  ::WideCharToMultiByte(CP_UTF8, 0, &tag_utf16[0], tag_utf16.size(), &tag[0],
                        str_size, nullptr, nullptr);

  return tag;
}

struct CallbackContext {
  TagEncoding encoding = TagEncoding::kUtf8;
  BinaryConstIt binary_begin;
  std::string tag;
};

// Callback used by PeImageReader::EnumCertificates(). If we find the tag,
// the function returns false to stop searching subsequent certificates.
bool SearchCertForTag(uint16_t revision,
                      uint16_t certificate_type,
                      const uint8_t* certificate_data,
                      size_t certificate_data_size,
                      void* context) {
  CallbackContext* callback_context =
      reinterpret_cast<CallbackContext*>(context);

  BinaryConstIt cert_it = callback_context->binary_begin +
                          (certificate_data - &*callback_context->binary_begin);

  switch (callback_context->encoding) {
    case TagEncoding::kUtf8:
      callback_context->tag =
          ReadTagUtf8(cert_it, cert_it + certificate_data_size);
      break;
    case TagEncoding::kUtf16:
      callback_context->tag =
          ReadTagUtf16(cert_it, cert_it + certificate_data_size);
      break;
  }

  return callback_context->tag.empty();
}

}  // namespace

BinaryConstIt AdvanceIt(BinaryConstIt it, size_t distance, BinaryConstIt end) {
  if (it >= end)
    return end;

  ptrdiff_t dist_to_end = 0;
  if (!base::CheckedNumeric<ptrdiff_t>(end - it).AssignIfValid(&dist_to_end))
    return end;

  return it + std::min(distance, static_cast<size_t>(dist_to_end));
}

bool CheckRange(BinaryConstIt it, size_t size, BinaryConstIt end) {
  if (it >= end || size == 0)
    return false;

  ptrdiff_t dist_to_end = 0;
  if (!base::CheckedNumeric<ptrdiff_t>(end - it).AssignIfValid(&dist_to_end))
    return false;

  return size <= static_cast<size_t>(dist_to_end);
}

std::string ExtractTagFromBuffer(const std::vector<uint8_t>& binary,
                                 TagEncoding encoding) {
  base::win::PeImageReader pe_image_reader;

  if (!pe_image_reader.Initialize(&binary[0], binary.size()))
    return std::string();

  CallbackContext context;
  context.encoding = encoding;
  context.binary_begin = binary.begin();

  pe_image_reader.EnumCertificates(SearchCertForTag, &context);

  return context.tag;
}

std::string ExtractTagFromFile(const std::wstring& path, TagEncoding encoding) {
  HANDLE file_handle =
      ::CreateFile(path.c_str(), FILE_READ_ATTRIBUTES | FILE_READ_DATA,
                   FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file_handle == INVALID_HANDLE_VALUE)
    return std::string();

  LARGE_INTEGER file_size = {};
  if (!::GetFileSizeEx(file_handle, &file_size)) {
    ::CloseHandle(file_handle);
    return std::string();
  }

  if (file_size.QuadPart > std::numeric_limits<DWORD>::max()) {
    ::CloseHandle(file_handle);
    return std::string();
  }

  std::vector<uint8_t> binary(file_size.QuadPart);

  DWORD bytes_read = 0;
  if (!::ReadFile(file_handle, &binary[0], static_cast<DWORD>(binary.size()),
                  &bytes_read, nullptr)) {
    ::CloseHandle(file_handle);
    return std::string();
  }

  ::CloseHandle(file_handle);

  if (bytes_read < binary.size())
    return std::string();

  return ExtractTagFromBuffer(binary, encoding);
}

}  // namespace updater
