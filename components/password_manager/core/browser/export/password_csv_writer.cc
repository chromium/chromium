// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_csv_writer.h"

#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/export/csv_writer.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

namespace {

const char kTitleColumnName[] = "name";
const char kUrlColumnName[] = "url";
const char kUsernameColumnName[] = "username";
const char kPasswordColumnName[] = "password";
const char kNoteColumnName[] = "note";

// Converts |credential| into a single line in the CSV format. Metadata are
// lost, see SerializePasswords.
std::map<std::string, std::string> PasswordFormToRecord(
    const CredentialUIEntry& credential,
    const CredentialUIEntry::DomainInfo& domain) {
  std::map<std::string, std::string> record;
  record[kTitleColumnName] = domain.name;
  if (affiliations::IsValidAndroidFacetURI(domain.signon_realm)) {
    record[kUrlColumnName] = domain.signon_realm;
  } else {
    record[kUrlColumnName] = domain.url.spec();
  }
  record[kUsernameColumnName] = base::UTF16ToUTF8(credential.username);
  record[kPasswordColumnName] = base::UTF16ToUTF8(credential.password);
  record[kNoteColumnName] = base::UTF16ToUTF8(credential.note);
  return record;
}

}  // namespace

// static
std::string PasswordCSVWriter::SerializePasswords(
    const std::vector<CredentialUIEntry>& credentials) {
  std::vector<std::string> header(5);
  header[0] = kTitleColumnName;
  header[1] = kUrlColumnName;
  header[2] = kUsernameColumnName;
  header[3] = kPasswordColumnName;
  header[4] = kNoteColumnName;

  std::vector<std::map<std::string, std::string>> records;
  records.reserve(credentials.size());
  for (const auto& credential : credentials) {
    for (const auto& domain : credential.GetAffiliatedDomains()) {
      records.push_back(PasswordFormToRecord(credential, domain));
    }
  }

  std::sort(records.begin(), records.end(),
            [&header](const std::map<std::string, std::string>& lhs,
                      const std::map<std::string, std::string>& rhs) {
              for (const std::string& headerVal : header) {
                if (lhs.at(headerVal) < rhs.at(headerVal)) {
                  return true;
                }
                if (lhs.at(headerVal) > rhs.at(headerVal)) {
                  return false;
                }
              }
              return false;
            });

  std::string result;
  WriteCSV(header, records, &result);
  return result;
}

}  // namespace password_manager
