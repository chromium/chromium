// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/csv_password_parser_impl.h"

#include <Foundation/Foundation.h>

#include <utility>
#include <vector>

#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/password_manager/services/csv_password/ios/csv_passwords_parser_swift.h"

namespace {

password_manager::mojom::CSVPasswordSequencePtr DoParseCSV(
    const std::string& raw_csv) {
  if (raw_csv.empty()) {
    return nullptr;
  }

  CSVPasswordsParser* parser =
      [CSVPasswordsParser fromCSVInput:base::SysUTF8ToNSString(raw_csv)];
  if (!parser.passwords) {
    return nullptr;
  }

  password_manager::mojom::CSVPasswordSequencePtr result =
      password_manager::mojom::CSVPasswordSequence::New();
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

  return result;
}

}  // namespace

namespace password_manager {

CSVPasswordParserImpl::CSVPasswordParserImpl(
    mojo::PendingReceiver<mojom::CSVPasswordParser> receiver)
    : receiver_(this, std::move(receiver)) {}

CSVPasswordParserImpl::~CSVPasswordParserImpl() = default;

void CSVPasswordParserImpl::ParseCSV(const std::string& raw_csv,
                                     ParseCSVCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&DoParseCSV, base::OwnedRef(raw_csv)),
      std::move(callback));
}

}  // namespace password_manager
