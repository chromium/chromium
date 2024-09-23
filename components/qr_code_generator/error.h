// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QR_CODE_GENERATOR_ERROR_H_
#define COMPONENTS_QR_CODE_GENERATOR_ERROR_H_

#include <stdint.h>

namespace qr_code_generator {

enum class Error : uint8_t {
  kUnknownError = 0,

  // Input string was too long.  See
  // https://www.qrcode.com/en/about/version.html for information capacity of QR
  // codes.
  kInputTooLong = 1,
};

}  // namespace qr_code_generator

#endif  // COMPONENTS_QR_CODE_GENERATOR_ERROR_H_
