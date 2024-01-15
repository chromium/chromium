// Copyright 2012 The Chromium Authors
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
#include "chrome/common/importer/importer_data_types.h"
#include "crypto/crypto_buildflags.h"
#include "sql/database.h"
#include "sql/statement.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include <pk11pub.h>
#include <pk11sdr.h>
#endif  // BUILDFLAG(USE_NSS_CERTS)

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
  std::u16string username_element;
  std::u16string password_element;
  std::string encrypted_username;
  std::string encrypted_password;
  std::string form_action;
};

namespace {

importer::ImportedPasswordForm CreateBlockedPasswordForm(
    const std::string& blocked_host) {
  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();
  rep.ClearUsername();
  rep.ClearPassword();

  importer::ImportedPasswordForm form;
  form.url = GURL(blocked_host).ReplaceComponents(rep);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.blocked_by_user = true;
  return form;
}

}  // namespace

std::u16string NSSDecryptor::Decrypt(const std::string& crypt) const {
  // Do nothing if NSS is not loaded.
  if (!is_nss_initialized_)
    return std::u16string();

  if (crypt.empty())
    return std::u16string();

  // The old style password is encoded in base64. They are identified
  // by a leading '~'. Otherwise, we should decrypt the text.
  std::string plain;
  if (crypt[0] != '~') {
    std::string decoded_data;
    if (!base::Base64Decode(crypt, &decoded_data))
      return std::u16string();
    PK11SlotInfo* slot = GetKeySlotForDB();
    SECStatus result = PK11_Authenticate(slot, PR_TRUE, nullptr);
    if (result != SECSuccess) {
      FreeSlot(slot);
      return std::u16string();
    }

    SECItem request;
    request.data = reinterpret_cast<unsigned char*>(
        const_cast<char*>(decoded_data.data()));
    request.len = static_cast<unsigned int>(decoded_data.size());
    SECItem reply;
    reply.data = nullptr;
    reply.len = 0;
#if BUILDFLAG(USE_NSS_CERTS)
    result = PK11SDR_DecryptWithSlot(slot, &request, &reply, nullptr);
#else
    result = PK11SDR_Decrypt(&request, &reply, NULL);
#endif  // BUILDFLAG(USE_NSS_CERTS)
    if (result == SECSuccess)
      plain.assign(reinterpret_cast<char*>(reply.data), reply.len);

    SECITEM_FreeItem(&reply, PR_FALSE);
    FreeSlot(slot);
  } else {
    // Deletes the leading '~' before decoding.
    if (!base::Base64Decode(crypt.substr(1), &plain))
      return std::u16string();
  }

  return base::UTF8ToUTF16(plain);
}

bool NSSDecryptor::ReadAndParseLogins(
    const base::FilePath& json_file,
    std::vector<importer::ImportedPasswordForm>* forms) {
  std::string json_content;
  base::ReadFileToString(json_file, &json_content);
  std::optional<base::Value> parsed_json = base::JSONReader::Read(json_content);
  if (!parsed_json) {
    return false;
  }

  const base::Value::Dict* parsed_json_dict = parsed_json->GetIfDict();
  if (!parsed_json_dict) {
    return false;
  }

  const base::Value::List* disabled_hosts =
      parsed_json_dict->FindList("disabledHosts");
  if (disabled_hosts) {
    for (const auto& value : *disabled_hosts) {
      if (!value.is_string())
        continue;
      forms->push_back(CreateBlockedPasswordForm(value.GetString()));
    }
  }

  const base::Value::List* password_list = parsed_json_dict->FindList("logins");
  if (password_list) {
    for (const auto& value : *password_list) {
      auto* dict = value.GetIfDict();
      if (!dict) {
        continue;
      }

      FirefoxRawPasswordInfo raw_password_info;

      if (const std::string* hostname = dict->FindString("hostname")) {
        raw_password_info.host = *hostname;
      }

      if (const std::string* username = dict->FindString("usernameField")) {
        raw_password_info.username_element = base::UTF8ToUTF16(*username);
      }

      if (const std::string* password = dict->FindString("passwordField")) {
        raw_password_info.password_element = base::UTF8ToUTF16(*password);
      }

      if (const std::string* username = dict->FindString("encryptedUsername")) {
        raw_password_info.encrypted_username = *username;
      }

      if (const std::string* password = dict->FindString("encryptedPassword")) {
        raw_password_info.encrypted_password = *password;
      }

      if (const std::string* submit_url = dict->FindString("formSubmitURL")) {
        raw_password_info.form_action = *submit_url;
      }

      if (const std::string* realm = dict->FindString("httpRealm")) {
        raw_password_info.realm = *realm;
      }

      importer::ImportedPasswordForm form;
      if (CreatePasswordFormFromRawInfo(raw_password_info, &form))
        forms->push_back(form);
    }
  }

  return true;
}

bool NSSDecryptor::CreatePasswordFormFromRawInfo(
    const FirefoxRawPasswordInfo& raw_password_info,
    importer::ImportedPasswordForm* form) {
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

  form->url = url.ReplaceComponents(rep);
  form->signon_realm = form->url.DeprecatedGetOriginAsURL().spec();
  if (!raw_password_info.realm.empty()) {
    form->signon_realm += raw_password_info.realm;
    // Non-empty realm indicates that it's not html form authentication entry.
    // Extracted data doesn't allow us to distinguish basic_auth entry from
    // digest_auth entry, so let's assume basic_auth.
    form->scheme = importer::ImportedPasswordForm::Scheme::kBasic;
  }
  form->username_element = raw_password_info.username_element;
  form->username_value = Decrypt(raw_password_info.encrypted_username);
  form->password_element = raw_password_info.password_element;
  form->password_value = Decrypt(raw_password_info.encrypted_password);
  form->action = GURL(raw_password_info.form_action).ReplaceComponents(rep);

  return true;
}
