// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_TYPE_CONVERSIONS_H_
#define CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_TYPE_CONVERSIONS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"

namespace ipp_converter {

// Common converters for working with arbitrary byte buffers.
std::vector<uint8_t> ConvertToByteBuffer(std::string_view char_buffer);

}  // namespace ipp_converter

#endif  // CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_TYPE_CONVERSIONS_H_
