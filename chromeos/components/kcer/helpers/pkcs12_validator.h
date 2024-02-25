// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains a group of functions which are used for pkcs12 data
// validation before data import to chaps. They are used in kcer_chaps_util.cc,
// but they are not related to chaps, so they were moved to a separate file.

#ifndef CHROMEOS_COMPONENTS_KCER_HELPERS_PKCS12_VALIDATOR_H_
#define CHROMEOS_COMPONENTS_KCER_HELPERS_PKCS12_VALIDATOR_H_

#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "chromeos/components/kcer/helpers/pkcs12_reader.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/stack.h"

namespace kcer::internal {

// Returns an error message corresponding to the given import error code.
std::string MakePkcs12CertImportErrorMessage(Pkcs12ReaderStatusCode error_code);

// Prepares nickname (friendlyName/alias) for the certificate (`cert`)
// using several attempts - searches for the same DN (Distinguished name) among
// certificates in slot (`slot`) for reusing it. Then extracts nickname from
// certificate itself, then gets CommonName attribute and at the end assign some
// default nickname.
Pkcs12ReaderStatusCode GetNickname(PK11SlotInfo* slot,
                                   X509* cert,
                                   const Pkcs12Reader* pkcs12_reader,
                                   std::string& cert_nickname);

// Filter out certs from (`certs`) which are not directly related to key_data
// (`key_data`), extracts nickname from the certificate or placing default
// nickname and stores certificates which will be installed to
// (`valid_certs_data`).
Pkcs12ReaderStatusCode ValidateAndPrepareCertData(
    PK11SlotInfo* slot,
    const Pkcs12Reader& pkcs12_reader,
    bssl::UniquePtr<STACK_OF(X509)> certs,
    KeyData& key_data,
    std::vector<CertData>& valid_certs_data);

// Checks if private key is already present in slot (`slot`) by searching
// for the key using certificate (`cert`) and setting result to
// (`is_key_installed`).
Pkcs12ReaderStatusCode CanFindInstalledKey(PK11SlotInfo* slot,
                                           const CertData& cert,
                                           const Pkcs12Reader& pkcs12_reader,
                                           bool& is_key_installed);

}  // namespace kcer::internal

#endif  // CHROMEOS_COMPONENTS_KCER_HELPERS_PKCS12_VALIDATOR_H_
