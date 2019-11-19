// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_TYPE_CONVERSIONS_H_
#define CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_TYPE_CONVERSIONS_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"

namespace ipp_converter {

// Common converters for working with arbitrary byte buffers.
std::vector<uint8_t> ConvertToByteBuffer(base::StringPiece char_buffer);
std::vector<char> ConvertToCharBuffer(base::span<const uint8_t> byte_buffer);
std::string ConvertToString(base::span<const uint8_t> byte_buffer);

}  // namespace ipp_converter

#endif  // CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_TYPE_CONVERSIONS_H_
