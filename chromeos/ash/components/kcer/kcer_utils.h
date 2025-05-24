// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KCER_KCER_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_KCER_KCER_UTILS_H_

#include "chromeos/ash/components/kcer/kcer.h"

namespace kcer {

// Generate a vector with all the signing schemes that a key can perform based
// on the `key_type` and whether the token supports PSS.
std::vector<SigningScheme> GetSupportedSigningSchemes(bool supports_pss,
                                                      KeyType key_type);

// The EC signature returned by Chaps is a concatenation of two numbers r and s
// (see PKCS#11 v2.40: 2.3.1 EC Signatures). Kcer needs to return it as a DER
// encoding of the following ASN.1 notations:
// Ecdsa-Sig-Value ::= SEQUENCE {
//     r       INTEGER,
//     s       INTEGER
// }
// (according to the RFC 8422, Section 5.4).
// This function reencodes the signature.
COMPONENT_EXPORT(KCER)
base::expected<std::vector<uint8_t>, Error> ReencodeEcSignatureAsAsn1(
    base::span<const uint8_t> signature);

}  // namespace kcer

#endif  // CHROMEOS_ASH_COMPONENTS_KCER_KCER_UTILS_H_
