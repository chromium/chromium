// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser_traits.h"

#include "components/password_manager/core/browser/import/csv_password.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

password_manager::mojom::CSVPassword_Status
EnumTraits<password_manager::mojom::CSVPassword_Status,
           password_manager::CSVPassword::Status>::
    ToMojom(password_manager::CSVPassword::Status status) {
  switch (status) {
    case password_manager::CSVPassword::Status::kOK:
      return password_manager::mojom::CSVPassword_Status::kOK;
    case password_manager::CSVPassword::Status::kSyntaxError:
      return password_manager::mojom::CSVPassword_Status::kSyntaxError;
    case password_manager::CSVPassword::Status::kSemanticError:
      return password_manager::mojom::CSVPassword_Status::kSemanticError;
  }
  NOTREACHED_IN_MIGRATION();
  return password_manager::mojom::CSVPassword_Status::kSyntaxError;
}

bool EnumTraits<password_manager::mojom::CSVPassword_Status,
                password_manager::CSVPassword::Status>::
    FromMojom(password_manager::mojom::CSVPassword_Status status,
              password_manager::CSVPassword::Status* out) {
  switch (status) {
    case password_manager::mojom::CSVPassword_Status::kOK:
      *out = password_manager::CSVPassword::Status::kOK;
      return true;
    case password_manager::mojom::CSVPassword_Status::kSyntaxError:
      *out = password_manager::CSVPassword::Status::kSyntaxError;
      return true;
    case password_manager::mojom::CSVPassword_Status::kSemanticError:
      *out = password_manager::CSVPassword::Status::kSemanticError;
      return true;
  }
  return false;
}

// static
bool StructTraits<password_manager::mojom::CSVPasswordDataView,
                  password_manager::CSVPassword>::
    Read(password_manager::mojom::CSVPasswordDataView data,
         password_manager::CSVPassword* out) {
  password_manager::CSVPassword::Status status;
  GURL url;
  std::string username;
  std::string password;
  std::string note;

  if (!data.ReadStatus(&status))
    return false;
  if (!data.ReadUrl(&url))
    return false;
  if (!data.ReadUsername(&username))
    return false;
  if (!data.ReadPassword(&password))
    return false;
  if (!data.ReadNote(&note))
    return false;
  if (url.is_valid()) {
    *out = password_manager::CSVPassword(url, username, password, note, status);
    return true;
  }
  std::optional<std::string> invalid_url;
  if (!data.ReadInvalidUrl(&invalid_url))
    return false;
  DCHECK(invalid_url.has_value());
  *out = password_manager::CSVPassword(invalid_url.value(), username, password,
                                       note, status);
  return true;
}

}  // namespace mojo
