// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_csv_writer.h"

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/export/csv_writer.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

namespace {

const char kTitleColumnName[] = "name";
const char kUrlColumnName[] = "url";
const char kUsernameColumnName[] = "username";
const char kPasswordColumnName[] = "password";

}  // namespace

// static
std::string PasswordCSVWriter::SerializePasswords(
    const std::vector<std::unique_ptr<PasswordForm>>& passwords) {
  std::vector<std::string> header(4);
  header[0] = kTitleColumnName;
  header[1] = kUrlColumnName;
  header[2] = kUsernameColumnName;
  header[3] = kPasswordColumnName;

  std::vector<std::map<std::string, std::string>> records;
  records.reserve(passwords.size());
  for (const auto& password : passwords) {
    records.push_back(PasswordFormToRecord(*password));
  }

  std::string result;
  WriteCSV(header, records, &result);
  return result;
}

std::map<std::string, std::string> PasswordCSVWriter::PasswordFormToRecord(
    const PasswordForm& form) {
  std::map<std::string, std::string> record;
  record[kUrlColumnName] = form.url.spec();
  record[kUsernameColumnName] = base::UTF16ToUTF8(form.username_value);
  record[kPasswordColumnName] = base::UTF16ToUTF8(form.password_value);
  record[kTitleColumnName] = form.url.host();
  return record;
}

}  // namespace password_manager
