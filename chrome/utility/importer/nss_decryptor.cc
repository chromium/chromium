// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/nss_decryptor.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form.h"
#include "sql/database.h"
#include "sql/statement.h"

#if defined(USE_NSS_CERTS)
#include <pk11pub.h>
#include <pk11sdr.h>
#endif  // defined(USE_NSS_CERTS)

// This method is based on some Firefox code in
//   security/manager/ssl/src/nsSDR.cpp
// The license block is:

/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is the Netscape security libraries.
*
* The Initial Developer of the Original Code is
* Netscape Communications Corporation.
* Portions created by the Initial Developer are Copyright (C) 1994-2000
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the MPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the MPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

// Use this structure to store unprocessed information extracted from
// Firefox's password file.
struct FirefoxRawPasswordInfo {
  std::string host;
  std::string realm;
  base::string16 username_element;
  base::string16 password_element;
  std::string encrypted_username;
  std::string encrypted_password;
  std::string form_action;
};

namespace {

autofill::PasswordForm CreateBlacklistPasswordForm(
    const std::string& blacklist_host) {
  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();
  rep.ClearUsername();
  rep.ClearPassword();

  autofill::PasswordForm form;
  form.origin = GURL(blacklist_host).ReplaceComponents(rep);
  form.signon_realm = form.origin.GetOrigin().spec();
  form.blacklisted_by_user = true;
  return form;
}

}  // namespace

base::string16 NSSDecryptor::Decrypt(const std::string& crypt) const {
  // Do nothing if NSS is not loaded.
  if (!is_nss_initialized_)
    return base::string16();

  if (crypt.empty())
    return base::string16();

  // The old style password is encoded in base64. They are identified
  // by a leading '~'. Otherwise, we should decrypt the text.
  std::string plain;
  if (crypt[0] != '~') {
    std::string decoded_data;
    if (!base::Base64Decode(crypt, &decoded_data))
      return base::string16();
    PK11SlotInfo* slot = GetKeySlotForDB();
    SECStatus result = PK11_Authenticate(slot, PR_TRUE, NULL);
    if (result != SECSuccess) {
      FreeSlot(slot);
      return base::string16();
    }

    SECItem request;
    request.data = reinterpret_cast<unsigned char*>(
        const_cast<char*>(decoded_data.data()));
    request.len = static_cast<unsigned int>(decoded_data.size());
    SECItem reply;
    reply.data = NULL;
    reply.len = 0;
#if defined(USE_NSS_CERTS)
    result = PK11SDR_DecryptWithSlot(slot, &request, &reply, NULL);
#else
    result = PK11SDR_Decrypt(&request, &reply, NULL);
#endif  // defined(USE_NSS_CERTS)
    if (result == SECSuccess)
      plain.assign(reinterpret_cast<char*>(reply.data), reply.len);

    SECITEM_FreeItem(&reply, PR_FALSE);
    FreeSlot(slot);
  } else {
    // Deletes the leading '~' before decoding.
    if (!base::Base64Decode(crypt.substr(1), &plain))
      return base::string16();
  }

  return base::UTF8ToUTF16(plain);
}

// There are three versions of password files. They store saved user
// names and passwords.
// References:
// http://kb.mozillazine.org/Signons.txt
// http://kb.mozillazine.org/Signons2.txt
// http://kb.mozillazine.org/Signons3.txt
void NSSDecryptor::ParseSignons(const base::FilePath& signon_file,
                                std::vector<autofill::PasswordForm>* forms) {
  forms->clear();

  std::string content;
  base::ReadFileToString(signon_file, &content);

  // Splits the file content into lines.
  std::vector<std::string> lines = base::SplitString(
      content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // The first line is the file version. We skip the unknown versions.
  if (lines.empty())
    return;
  int version;
  if (lines[0] == "#2c")
    version = 1;
  else if (lines[0] == "#2d")
    version = 2;
  else if (lines[0] == "#2e")
    version = 3;
  else
    return;

  // Reads never-saved list. Domains are stored one per line.
  size_t i;
  for (i = 1; i < lines.size() && lines[i].compare(".") != 0; ++i)
    forms->push_back(CreateBlacklistPasswordForm(lines[i]));
  ++i;

  // Reads saved passwords. The information is stored in blocks
  // seperated by lines that only contain a dot. We find a block
  // by the seperator and parse them one by one.
  while (i < lines.size()) {
    size_t begin = i;
    size_t end = i + 1;
    while (end < lines.size() && lines[end].compare(".") != 0)
      ++end;
    i = end + 1;

    // A block has at least five lines.
    if (end - begin < 5)
      continue;

    FirefoxRawPasswordInfo raw_password_info;

    // The first line is the site URL.
    // For HTTP authentication logins, the URL may contain http realm,
    // which will be in bracket:
    //   sitename:8080 (realm)
    const char kRealmBracketBegin[] = " (";
    const char kRealmBracketEnd[] = ")";
    if (lines[begin].find(kRealmBracketBegin) != std::string::npos) {
      size_t start = lines[begin].find(kRealmBracketBegin);
      raw_password_info.host = lines[begin].substr(0, start);
      start += sizeof(kRealmBracketBegin) - 1;
      size_t end = lines[begin].rfind(kRealmBracketEnd);
      raw_password_info.realm = lines[begin].substr(start, end - start);
    } else {
      raw_password_info.host = lines[begin];
    }

    ++begin;

    // There may be multiple username/password pairs for this site.
    // In this case, they are saved in one block without a seperated
    // line (contains a dot).
    while (begin + 4 < end) {
      // The user name.
      raw_password_info.username_element = base::UTF8ToUTF16(lines[begin++]);
      raw_password_info.encrypted_username = lines[begin++];
      // The element name has a leading '*'.
      if (lines[begin].at(0) == '*') {
        raw_password_info.password_element =
            base::UTF8ToUTF16(lines[begin++].substr(1));
        raw_password_info.encrypted_password = lines[begin++];
      } else {
        // Maybe the file is bad, we skip to next block.
        break;
      }
      // The action attribute from the form element. This line exists
      // in versin 2 or above.
      if (version >= 2) {
        if (begin < end)
          raw_password_info.form_action = lines[begin];
        ++begin;
      }
      // Version 3 has an extra line for further use.
      if (version == 3)
        ++begin;

      autofill::PasswordForm form;
      if (CreatePasswordFormFromRawInfo(raw_password_info, &form))
        forms->push_back(form);
    }
  }
}

bool NSSDecryptor::ReadAndParseSignons(
    const base::FilePath& sqlite_file,
    std::vector<autofill::PasswordForm>* forms) {
  sql::Database db;
  if (!db.Open(sqlite_file))
    return false;

  const char query[] = "SELECT hostname FROM moz_disabledHosts";
  sql::Statement s(db.GetUniqueStatement(query));
  if (!s.is_valid())
    return false;

  // Read domains for which passwords are never saved.
  while (s.Step())
    forms->push_back(CreateBlacklistPasswordForm(s.ColumnString(0)));

  const char query2[] = "SELECT hostname, httpRealm, formSubmitURL, "
                        "usernameField, passwordField, encryptedUsername, "
                        "encryptedPassword FROM moz_logins";

  sql::Statement s2(db.GetUniqueStatement(query2));
  if (!s2.is_valid())
    return false;

  while (s2.Step()) {
    FirefoxRawPasswordInfo raw_password_info;
    raw_password_info.host = s2.ColumnString(0);
    raw_password_info.realm = s2.ColumnString(1);
    // The user name, password and action.
    raw_password_info.username_element = s2.ColumnString16(3);
    raw_password_info.encrypted_username = s2.ColumnString(5);
    raw_password_info.password_element = s2.ColumnString16(4);
    raw_password_info.encrypted_password = s2.ColumnString(6);
    raw_password_info.form_action = s2.ColumnString(2);
    autofill::PasswordForm form;
    if (CreatePasswordFormFromRawInfo(raw_password_info, &form))
      forms->push_back(form);
  }
  return true;
}

bool NSSDecryptor::ReadAndParseLogins(
    const base::FilePath& json_file,
    std::vector<autofill::PasswordForm>* forms) {
  std::string json_content;
  base::ReadFileToString(json_file, &json_content);
  base::Optional<base::Value> parsed_json =
      base::JSONReader::Read(json_content);
  if (!parsed_json || !parsed_json->is_dict())
    return false;

  const base::Value* blacklist_domains =
      parsed_json->FindListKey("disabledHosts");
  if (blacklist_domains) {
    for (const auto& value : blacklist_domains->GetList()) {
      if (!value.is_string())
        continue;
      forms->push_back(CreateBlacklistPasswordForm(value.GetString()));
    }
  }

  const base::Value* password_list = parsed_json->FindListKey("logins");
  if (password_list) {
    for (const auto& value : password_list->GetList()) {
      if (!value.is_dict())
        continue;

      FirefoxRawPasswordInfo raw_password_info;

      if (const std::string* hostname = value.FindStringKey("hostname"))
        raw_password_info.host = *hostname;

      if (const std::string* username = value.FindStringKey("usernameField"))
        raw_password_info.username_element = base::UTF8ToUTF16(*username);

      if (const std::string* password = value.FindStringKey("passwordField"))
        raw_password_info.password_element = base::UTF8ToUTF16(*password);

      if (const std::string* username =
              value.FindStringKey("encryptedUsername"))
        raw_password_info.encrypted_username = *username;

      if (const std::string* password =
              value.FindStringKey("encryptedPassword"))
        raw_password_info.encrypted_password = *password;

      if (const std::string* submit_url = value.FindStringKey("formSubmitURL"))
        raw_password_info.form_action = *submit_url;

      if (const std::string* realm = value.FindStringKey("httpRealm"))
        raw_password_info.realm = *realm;

      autofill::PasswordForm form;
      if (CreatePasswordFormFromRawInfo(raw_password_info, &form))
        forms->push_back(form);
    }
  }

  return true;
}

bool NSSDecryptor::CreatePasswordFormFromRawInfo(
    const FirefoxRawPasswordInfo& raw_password_info,
    autofill::PasswordForm* form) {
  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();
  rep.ClearUsername();
  rep.ClearPassword();

  GURL url;
  if (!raw_password_info.realm.empty() &&
      raw_password_info.host.find("://") == std::string::npos) {
    // Assume HTTP for forms with non-empty realm and no scheme in hostname.
    url = GURL("http://" + raw_password_info.host);
  } else {
    url = GURL(raw_password_info.host);
  }
  // Skip this login if the URL is not valid.
  if (!url.is_valid())
    return false;

  form->origin = url.ReplaceComponents(rep);
  form->signon_realm = form->origin.GetOrigin().spec();
  if (!raw_password_info.realm.empty()) {
    form->signon_realm += raw_password_info.realm;
    // Non-empty realm indicates that it's not html form authentication entry.
    // Extracted data doesn't allow us to distinguish basic_auth entry from
    // digest_auth entry, so let's assume basic_auth.
    form->scheme = autofill::PasswordForm::Scheme::kBasic;
  }
  form->username_element = raw_password_info.username_element;
  form->username_value = Decrypt(raw_password_info.encrypted_username);
  form->password_element = raw_password_info.password_element;
  form->password_value = Decrypt(raw_password_info.encrypted_password);
  form->action = GURL(raw_password_info.form_action).ReplaceComponents(rep);

  return true;
}
