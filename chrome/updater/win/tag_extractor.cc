// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/tag_extractor.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "base/numerics/checked_math.h"
#include "base/win/pe_image_reader.h"
#include "build/build_config.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/win/tag_extractor_impl.h"

namespace updater {

namespace {

struct CallbackContext {
  TagEncoding encoding = TagEncoding::kUtf8;
  std::vector<uint8_t>::const_iterator binary_begin;
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

  std::vector<uint8_t>::const_iterator cert_it =
      callback_context->binary_begin +
      (certificate_data - &*callback_context->binary_begin);

  switch (callback_context->encoding) {
    case TagEncoding::kUtf8:
      callback_context->tag =
          tagging::ReadTagUtf8(cert_it, cert_it + certificate_data_size);
      break;
    case TagEncoding::kUtf16:
      callback_context->tag =
          tagging::ReadTagUtf16(cert_it, cert_it + certificate_data_size);
      break;
  }

  return callback_context->tag.empty();
}

}  // namespace

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
