// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_PKCS11_DEFINITIONS_H_
#define CHROMEOS_CONSTANTS_PKCS11_DEFINITIONS_H_

#include <stdint.h>

// This file provides types and constants defined in the PKCS#11 standard. The
// PKCS11_ prefix is needed to avoid name collisions with #define-d types and
// constants from the NSS library.

namespace chromeos {

// PKCS #11 v2.20 section 5 page 11.
using PKCS11_CK_BYTE = unsigned char;
using PKCS11_CK_BBOOL = PKCS11_CK_BYTE;
using PKCS11_CK_ULONG = unsigned long int;
// PKCS #11 v2.20 section 9.4 pages 48-50.
using PKCS11_CK_OBJECT_CLASS = PKCS11_CK_ULONG;
using PKCS11_CK_KEY_TYPE = PKCS11_CK_ULONG;
using PKCS11_CK_ATTRIBUTE_TYPE = PKCS11_CK_ULONG;
using PKCS11_CK_CERTIFICATE_TYPE = PKCS11_CK_ULONG;
// PKCS #11 v2.20 section 9.5 page 52.
using PKCS11_CK_MECHANISM_TYPE = PKCS11_CK_ULONG;
// PKCS #11 v2.20 section 12.1.6 page 198.
using PKCS11_CK_RSA_PKCS_MGF_TYPE = PKCS11_CK_ULONG;

// PKCS #11 v2.20 section 12.1.8 page 201.
struct PKCS11_CK_RSA_PKCS_PSS_PARAMS {
  PKCS11_CK_MECHANISM_TYPE hashAlg;
  PKCS11_CK_RSA_PKCS_MGF_TYPE mgf;
  PKCS11_CK_ULONG sLen;
};

// PKCS #11 v2.20 section 5 page 12.
inline constexpr PKCS11_CK_BBOOL PKCS11_CK_FALSE = 0;
inline constexpr PKCS11_CK_BBOOL PKCS11_CK_TRUE = 1;

// PKCS #11 v2.20 section 6.7.5 page 23, valid session ids are non-zero.
inline constexpr uint64_t PKCS11_INVALID_SESSION_ID = 0;

// PKCS #11 v2.20 section 9.3 page 48.
inline constexpr uint32_t PKCS11_CKF_RW_SESSION = 0x00000002;
inline constexpr uint32_t PKCS11_CKF_SERIAL_SESSION = 0x00000004;

// PKCS #11 v2.20 section A Manifest constants page 375.
inline constexpr uint32_t PKCS11_CK_UNAVAILABLE_INFORMATION =
    static_cast<uint32_t>(~0UL);

// PKCS #11 v2.20 section A Manifest constants page 375.
inline constexpr uint32_t PKCS11_CKO_CERTIFICATE = 0x00000001;
inline constexpr uint32_t PKCS11_CKO_PUBLIC_KEY = 0x00000002;
inline constexpr uint32_t PKCS11_CKO_PRIVATE_KEY = 0x00000003;

// PKCS #11 v2.20 section A Manifest constants page 375.
inline constexpr uint32_t PKCS11_CKK_RSA = 0x00000000;
inline constexpr uint32_t PKCS11_CKK_EC = 0x00000003;

// PKCS #11 v2.20 section A Manifest constants page 376.
inline constexpr uint32_t PKCS11_CKC_X_509 = 0x00000000;

// PKCS #11 v2.20 section A Manifest constants pages 376-377.
inline constexpr uint32_t PKCS11_CKA_CLASS = 0x00000000;
inline constexpr uint32_t PKCS11_CKA_TOKEN = 0x00000001;
inline constexpr uint32_t PKCS11_CKA_PRIVATE = 0x00000002;
inline constexpr uint32_t PKCS11_CKA_LABEL = 0x00000003;
inline constexpr uint32_t PKCS11_CKA_VALUE = 0x00000011;
inline constexpr uint32_t PKCS11_CKA_CERTIFICATE_TYPE = 0x00000080;
inline constexpr uint32_t PKCS11_CKA_ISSUER = 0x00000081;
inline constexpr uint32_t PKCS11_CKA_SERIAL_NUMBER = 0x00000082;
inline constexpr uint32_t PKCS11_CKA_KEY_TYPE = 0x00000100;
inline constexpr uint32_t PKCS11_CKA_SUBJECT = 0x00000101;
inline constexpr uint32_t PKCS11_CKA_ID = 0x00000102;
inline constexpr uint32_t PKCS11_CKA_SENSITIVE = 0x00000103;
inline constexpr uint32_t PKCS11_CKA_ENCRYPT = 0x00000104;
inline constexpr uint32_t PKCS11_CKA_DECRYPT = 0x00000105;
inline constexpr uint32_t PKCS11_CKA_WRAP = 0x00000106;
inline constexpr uint32_t PKCS11_CKA_UNWRAP = 0x00000107;
inline constexpr uint32_t PKCS11_CKA_SIGN = 0x00000108;
inline constexpr uint32_t PKCS11_CKA_SIGN_RECOVER = 0x00000109;
inline constexpr uint32_t PKCS11_CKA_VERIFY = 0x0000010A;
inline constexpr uint32_t PKCS11_CKA_DERIVE = 0x0000010C;
// Should be used for CreateObject, GetAttributeValue.
inline constexpr uint32_t PKCS11_CKA_MODULUS = 0x00000120;
// Should be used for GenerateKeyPair.
inline constexpr uint32_t PKCS11_CKA_MODULUS_BITS = 0x00000121;
inline constexpr uint32_t PKCS11_CKA_PUBLIC_EXPONENT = 0x00000122;
inline constexpr uint32_t PKCS11_CKA_PRIVATE_EXPONENT = 0x00000123;
inline constexpr uint32_t PKCS11_CKA_PRIME_1 = 0x00000124;
inline constexpr uint32_t PKCS11_CKA_PRIME_2 = 0x00000125;
inline constexpr uint32_t PKCS11_CKA_EXPONENT_1 = 0x00000126;
inline constexpr uint32_t PKCS11_CKA_EXPONENT_2 = 0x00000127;
inline constexpr uint32_t PKCS11_CKA_COEFFICIENT = 0x00000128;
inline constexpr uint32_t PKCS11_CKA_EXTRACTABLE = 0x00000162;
inline constexpr uint32_t PKCS11_CKA_EC_PARAMS = 0x00000180;
inline constexpr uint32_t PKCS11_CKA_EC_POINT = 0x00000181;

// PKCS #11 v2.20 section A Manifest constants page 377-380.
inline constexpr uint32_t PKCS11_CKM_RSA_PKCS_KEY_PAIR_GEN = 0x00000000;
inline constexpr uint32_t PKCS11_CKM_RSA_PKCS = 0x00000001;
inline constexpr uint32_t PKCS11_CKM_RSA_PKCS_PSS = 0x0000000D;
inline constexpr uint32_t PKCS11_CKM_EC_KEY_PAIR_GEN = 0x00001040;
inline constexpr uint32_t PKCS11_CKM_ECDSA = 0x00001041;

// PKCS #11 v2.20 section A Manifest constants pages 381-382.
inline constexpr uint32_t PKCS11_CKR_OK = 0x00000000;
inline constexpr uint32_t PKCS11_CKR_GENERAL_ERROR = 0x00000005;
inline constexpr uint32_t PKCS11_CKR_ATTRIBUTE_TYPE_INVALID = 0x00000012;
inline constexpr uint32_t PKCS11_CKR_SESSION_CLOSED = 0x000000B0;
inline constexpr uint32_t PKCS11_CKR_SESSION_HANDLE_INVALID = 0x000000B3;
inline constexpr uint32_t PKCS11_CKR_BUFFER_TOO_SMALL = 0x00000150;

}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_PKCS11_DEFINITIONS_H_
