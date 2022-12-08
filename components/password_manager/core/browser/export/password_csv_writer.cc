// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_csv_writer.h"

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/export/csv_writer.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/sync/base/features.h"

namespace password_manager {

namespace {

const char kTitleColumnName[] = "name";
const char kUrlColumnName[] = "url";
const char kUsernameColumnName[] = "username";
const char kPasswordColumnName[] = "password";
const char kNoteColumnName[] = "note";

}  // namespace

// static
std::string PasswordCSVWriter::SerializePasswords(
    const std::vector<CredentialUIEntry>& credentials) {
  std::vector<std::string> header(4);
  header[0] = kTitleColumnName;
  header[1] = kUrlColumnName;
  header[2] = kUsernameColumnName;
  header[3] = kPasswordColumnName;
  if (base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
    header.resize(5);
    header[4] = kNoteColumnName;
  }

  std::vector<std::map<std::string, std::string>> records;
  records.reserve(credentials.size());
  for (const auto& credential : credentials) {
    records.push_back(PasswordFormToRecord(credential));
  }

  std::string result;
  WriteCSV(header, records, &result);
  return result;
}

std::map<std::string, std::string> PasswordCSVWriter::PasswordFormToRecord(
    const CredentialUIEntry& credential) {
  std::map<std::string, std::string> record;
  record[kTitleColumnName] = GetShownOrigin(credential);
  record[kUrlColumnName] = credential.GetURL().spec();
  record[kUsernameColumnName] = base::UTF16ToUTF8(credential.username);
  record[kPasswordColumnName] = base::UTF16ToUTF8(credential.password);
  if (base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
    record[kNoteColumnName] = base::UTF16ToUTF8(credential.note);
  }
  return record;
}

}  // namespace password_manager
