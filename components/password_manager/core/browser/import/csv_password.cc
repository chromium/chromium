// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password.h"

#include <string>
#include <utility>

#include "base/types/expected.h"
#include "url/gurl.h"

namespace password_manager {

CSVPassword::CSVPassword() : status_(Status::kSemanticError) {}

CSVPassword::CSVPassword(base::expected<GURL, std::string> url,
                         std::string username,
                         std::string password,
                         std::string note,
                         Status status)
    : url_(std::move(url)),
      username_(std::move(username)),
      password_(std::move(password)),
      note_(std::move(note)),
      status_(status) {}

CSVPassword::CSVPassword(GURL url,
                         std::string username,
                         std::string password,
                         std::string note,
                         Status status)
    : CSVPassword(base::expected<GURL, std::string>(std::move(url)),
                  std::move(username),
                  std::move(password),
                  std::move(note),
                  status) {}

CSVPassword::CSVPassword(std::string invalid_url,
                         std::string username,
                         std::string password,
                         std::string note,
                         Status status)
    : CSVPassword(base::unexpected(std::move(invalid_url)),
                  std::move(username),
                  std::move(password),
                  std::move(note),
                  status) {}

CSVPassword::CSVPassword(const CSVPassword&) = default;
CSVPassword::CSVPassword(CSVPassword&&) = default;
CSVPassword& CSVPassword::operator=(const CSVPassword&) = default;
CSVPassword& CSVPassword::operator=(CSVPassword&&) = default;
CSVPassword::~CSVPassword() = default;

}  // namespace password_manager
