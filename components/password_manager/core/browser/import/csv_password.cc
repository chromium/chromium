// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password.h"

#include <utility>

#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/import/csv_field_parser.h"

#include "url/gurl.h"

namespace password_manager {

namespace {

// Convert() unescapes a CSV field |str| and converts the result to a 16-bit
// string. |str| is assumed to exclude the outer pair of quotation marks, if
// originally present.
std::u16string Convert(base::StringPiece str) {
  std::string str_copy(str);
  base::ReplaceSubstringsAfterOffset(&str_copy, 0, "\"\"", "\"");
  return base::UTF8ToUTF16(str_copy);
}

}  // namespace

CSVPassword::CSVPassword(const ColumnMap& map, base::StringPiece row) {
  if (map.size() != kLabelCount) {
    status_ = Status::kSemanticError;
    return;
  }

  size_t field_idx = 0;
  CSVFieldParser parser(row);
  bool username_set = false;
  status_ = Status::kOK;

  while (parser.HasMoreFields()) {
    base::StringPiece field;
    if (!parser.NextField(&field)) {
      status_ = Status::kSyntaxError;
      return;
    }
    auto meaning_it = map.find(field_idx++);
    if (meaning_it == map.end())
      continue;
    switch (meaning_it->second) {
      case Label::kOrigin:
        if (!base::IsStringASCII(field)) {
          status_ = Status::kSyntaxError;
          return;
        }
        url_ = GURL(field);
        break;
      case Label::kUsername:
        username_ = field;
        username_set = true;
        break;
      case Label::kPassword:
        password_ = field;
        break;
    }
  }
  // While all of origin, username and password must be set in the CSV data
  // row, username is permitted to be an empty string, while password and
  // origin are not.
  if (!url_.is_valid() || !username_set || password_.empty()) {
    status_ = Status::kSemanticError;
  }
}

CSVPassword::Status CSVPassword::GetParseStatus() const {
  return status_;
}

PasswordForm CSVPassword::ToPasswordForm() const {
  // Only valid PasswordForms are allowed to be created.
  DCHECK_EQ(this->GetParseStatus(), Status::kOK);
  // There is currently no way to import non-HTML credentials.
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  // Android credentials have a non-standard scheme ("android://"). Hence the
  // following explicit check is necessary to set |signon_realm| correctly for
  // both regular and Android credentials.
  form.signon_realm =
      IsValidAndroidFacetURI(url_.spec()) ? url_.spec() : GetSignonRealm(url_);
  form.url = url_;
  form.username_value = Convert(username_);
  form.password_value = Convert(password_);
  form.date_created = base::Time::Now();
  form.date_password_modified = form.date_created;
  return form;
}

}  // namespace password_manager
