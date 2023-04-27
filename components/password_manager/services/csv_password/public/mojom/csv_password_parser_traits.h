// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_PUBLIC_MOJOM_CSV_PASSWORD_PARSER_TRAITS_H_
#define COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_PUBLIC_MOJOM_CSV_PASSWORD_PARSER_TRAITS_H_

#include "base/types/expected.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

template <>
struct EnumTraits<password_manager::mojom::CSVPassword_Status,
                  password_manager::CSVPassword::Status> {
  static password_manager::mojom::CSVPassword_Status ToMojom(
      password_manager::CSVPassword::Status status);
  static bool FromMojom(password_manager::mojom::CSVPassword_Status status,
                        password_manager::CSVPassword::Status* output);
};

template <>
struct StructTraits<password_manager::mojom::CSVPasswordDataView,
                    password_manager::CSVPassword> {
  static password_manager::CSVPassword::Status status(
      const password_manager::CSVPassword& r) {
    return r.GetParseStatus();
  }
  static const GURL url(const password_manager::CSVPassword& r) {
    return r.GetURL().value_or(GURL());
  }
  static absl::optional<std::string> invalid_url(
      const password_manager::CSVPassword& r) {
    base::expected<GURL, std::string> expected_url = r.GetURL();
    if (!expected_url.has_value())
      return expected_url.error();
    return absl::nullopt;
  }
  static const std::string& username(const password_manager::CSVPassword& r) {
    return r.GetUsername();
  }
  static const std::string& password(const password_manager::CSVPassword& r) {
    return r.GetPassword();
  }
  static const std::string& note(const password_manager::CSVPassword& r) {
    return r.GetNote();
  }
  static bool Read(password_manager::mojom::CSVPasswordDataView data,
                   password_manager::CSVPassword* out);
};

}  // namespace mojo

#endif  // COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_PUBLIC_MOJOM_CSV_PASSWORD_PARSER_TRAITS_H_
