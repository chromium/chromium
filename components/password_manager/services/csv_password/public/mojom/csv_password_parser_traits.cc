// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser_traits.h"

#include "components/password_manager/core/browser/import/csv_password.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<password_manager::mojom::CSVPasswordDataView,
                  password_manager::CSVPassword>::
    Read(password_manager::mojom::CSVPasswordDataView data,
         password_manager::CSVPassword* out) {
  GURL url;
  std::string username;
  std::string password;
  if (!data.ReadUrl(&url))
    return false;
  if (!data.ReadUsername(&username))
    return false;
  if (!data.ReadPassword(&password))
    return false;
  *out = password_manager::CSVPassword(url, username, password);
  return true;
}

}  // namespace mojo
