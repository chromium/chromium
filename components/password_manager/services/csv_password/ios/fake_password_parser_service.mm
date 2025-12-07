// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/fake_password_parser_service.h"

#include <Foundation/Foundation.h>

#include <utility>
#include <vector>

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/services/csv_password/ios/csv_passwords_parser_swift.h"

namespace password_manager {

// A wrapper on CSVPasswordSequence that mimics the sandbox behaviour.
void FakePasswordParserService::ParseCSV(const std::string& raw_csv,
                                         ParseCSVCallback callback) {
  mojom::CSVPasswordSequencePtr result = nullptr;

  if (raw_csv.empty()) {
    std::move(callback).Run(std::move(result));
    return;
  }

  CSVPasswordsParser* parser =
      [CSVPasswordsParser fromCSVInput:base::SysUTF8ToNSString(raw_csv)];
  if (!parser.passwords) {
    std::move(callback).Run(std::move(result));
    return;
  }

  result = mojom::CSVPasswordSequence::New();
  size_t count = [parser.passwords count];
  result->csv_passwords.reserve(count);
  for (PasswordData* parsed_password in parser.passwords) {
    std::string origin = base::SysNSStringToUTF8(parsed_password.origin);
    std::string username = base::SysNSStringToUTF8(parsed_password.username);
    std::string password = base::SysNSStringToUTF8(parsed_password.password);
    std::string note = base::SysNSStringToUTF8(parsed_password.note);
    GURL url(origin);
    if (url.is_valid()) {
      password_manager::CSVPassword csv_password(
          url, username, password, note,
          password_manager::CSVPassword::Status::kOK);
      result->csv_passwords.push_back(csv_password);
    } else {
      password_manager::CSVPassword csv_password(
          origin, username, password, note,
          password_manager::CSVPassword::Status::kOK);
      result->csv_passwords.push_back(csv_password);
    }
  }

  std::move(callback).Run(std::move(result));
}

}  // namespace password_manager
