// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/qr_code_generator/qr_code_generator.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span_rust.h"
#include "base/numerics/safe_conversions.h"
#include "components/qr_code_generator/cpp_api_from_rust_buildflags.h"

#if BUILDFLAG(ENABLE_CPP_API_FROM_RUST)
#include "third_party/crubit/support/rs_std/iterator_adapter.h"
#include "third_party/crubit/support/rs_std/slice_ref.h"
#include "third_party/rust/qr_code/v2/qr_code.h"
#else
#include "components/qr_code_generator/qr_code_generator_ffi_glue.rs.h"
#endif

namespace qr_code_generator {

GeneratedCode::GeneratedCode() = default;
GeneratedCode::~GeneratedCode() = default;
GeneratedCode::GeneratedCode(GeneratedCode&&) = default;
GeneratedCode& GeneratedCode::operator=(GeneratedCode&&) = default;

#if BUILDFLAG(ENABLE_CPP_API_FROM_RUST)
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
  std::ranges::transform(
      rs::IteratorAdapter<qr_code::QrCodeIterator>(rs_code.iter()),
      rs::IteratorEnd(), std::back_inserter(code.data),
      [](bool b) -> uint8_t { return b ? 1 : 0; });
  CHECK_EQ(code.data.size(), static_cast<size_t>(code.qr_size * code.qr_size));
  return code;
}
#else
base::expected<GeneratedCode, Error> GenerateCode(
    base::span<const uint8_t> in,
    std::optional<int> min_version) {
  rust::Slice<const uint8_t> rs_in = base::SpanToRustSlice(in);

  // `min_version` might come from a fuzzer and therefore we use a lenient
  // `saturated_cast` instead of a `checked_cast`.
  int16_t rs_min_version =
      base::saturated_cast<int16_t>(min_version.value_or(0));

  std::vector<uint8_t> result_pixels;
  size_t result_width = 0;
  Error result_error = Error::kUnknownError;
  bool result_is_success = generate_qr_code_using_rust(
      rs_in, rs_min_version, result_pixels, result_width, result_error);

  if (!result_is_success) {
    return base::unexpected(result_error);
  }
  GeneratedCode code;
  code.data = std::move(result_pixels);
  code.qr_size = base::checked_cast<int>(result_width);
  CHECK_EQ(code.data.size(), static_cast<size_t>(code.qr_size * code.qr_size));
  return code;
}
#endif

}  // namespace qr_code_generator
