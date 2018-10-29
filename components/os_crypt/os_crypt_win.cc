// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/os_crypt.h"

#include <windows.h>

#include "base/strings/utf_string_conversions.h"
#include "base/win/wincrypt_shim.h"

bool OSCrypt::EncryptString16(const base::string16& plaintext,
                              std::string* ciphertext) {
  return EncryptString(base::UTF16ToUTF8(plaintext), ciphertext);
}

bool OSCrypt::DecryptString16(const std::string& ciphertext,
                              base::string16* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}

bool OSCrypt::EncryptString(const std::string& plaintext,
                            std::string* ciphertext) {
  DATA_BLOB input;
  input.pbData = const_cast<BYTE*>(
      reinterpret_cast<const BYTE*>(plaintext.data()));
  input.cbData = static_cast<DWORD>(plaintext.length());

  DATA_BLOB output;
  BOOL result =
      CryptProtectData(&input, L"", nullptr, nullptr, nullptr, 0, &output);
  if (!result) {
    PLOG(ERROR) << "Failed to encrypt";
    return false;
  }

  // this does a copy
  ciphertext->assign(reinterpret_cast<std::string::value_type*>(output.pbData),
                     output.cbData);

  LocalFree(output.pbData);
  return true;
}

bool OSCrypt::DecryptString(const std::string& ciphertext,
                            std::string* plaintext) {
  DATA_BLOB input;
  input.pbData = const_cast<BYTE*>(
      reinterpret_cast<const BYTE*>(ciphertext.data()));
  input.cbData = static_cast<DWORD>(ciphertext.length());

  DATA_BLOB output;
  BOOL result = CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                                   0, &output);
  if (!result) {
    PLOG(ERROR) << "Failed to decrypt";
    return false;
  }

  plaintext->assign(reinterpret_cast<char*>(output.pbData), output.cbData);
  LocalFree(output.pbData);
  return true;
}
