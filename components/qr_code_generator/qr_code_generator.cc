// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/qr_code_generator/qr_code_generator.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/crubit/support/rs_std/iterator_adapter.h"
#include "third_party/crubit/support/rs_std/slice_ref.h"
#include "third_party/rust/qr_code/v2/qr_code.h"

namespace qr_code_generator {

GeneratedCode::GeneratedCode() = default;
GeneratedCode::~GeneratedCode() = default;
GeneratedCode::GeneratedCode(GeneratedCode&&) = default;
GeneratedCode& GeneratedCode::operator=(GeneratedCode&&) = default;

Error RustErrorToCppError(const ::qr_code::types::QrError& rust_error) {
  if (rust_error == ::qr_code::types::QrError::MakeDataTooLong()) {
    return Error::kInputTooLong;
  }
  return Error::kUnknownError;
}

base::expected<GeneratedCode, Error> GenerateCode(
    base::span<const uint8_t> in,
    std::optional<int> min_version) {
  if (min_version.has_value() && (*min_version < 1 || 40 < *min_version)) {
    return base::unexpected(Error::kUnknownError);
  }

  rs_std::SliceRef<const uint8_t> rs_in(in);
  auto result = ::qr_code::QrCode::new_(rs_in);
  if (!result.has_value()) {
    return base::unexpected(RustErrorToCppError(result.err()));
  }
  ::qr_code::QrCode rs_code = std::move(result).value();

  if (min_version.has_value()) {
    auto rs_min_version = ::qr_code::Version::MakeNormal(*min_version);
    if (rs_code.version().width() < rs_min_version.width()) {
      result = ::qr_code::QrCode::with_version(rs_in, std::move(rs_min_version),
                                               ::qr_code::EcLevel::MakeM());
      if (!result.has_value()) {
        return base::unexpected(RustErrorToCppError(result.err()));
      }
      rs_code = std::move(result).value();
    }
  }

  GeneratedCode code;
  code.qr_size = base::checked_cast<int>(rs_code.width());
  std::ranges::transform(rs::IteratorAdapter(rs_code.iter()), rs::IteratorEnd(),
                         std::back_inserter(code.data),
                         [](bool b) -> uint8_t { return b ? 1 : 0; });
  CHECK_EQ(code.data.size(), static_cast<size_t>(code.qr_size * code.qr_size));
  return code;
}

}  // namespace qr_code_generator
