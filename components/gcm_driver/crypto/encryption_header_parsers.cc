// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/encryption_header_parsers.h"

#include "base/base64url.h"
#include "base/strings/string_number_conversions.h"

#include "base/strings/string_util.h"


namespace gcm {

namespace {

// The default record size in bytes, as defined in section two of
// https://tools.ietf.org/html/draft-thomson-http-encryption.
const uint64_t kDefaultRecordSizeBytes = 4096;

// Decodes the string in |value| using base64url and writes the decoded value to
// |*salt|. Returns whether the string is not empty and could be decoded.
bool ValueToDecodedString(base::StringPiece value, std::string* salt) {
  if (value.empty())
    return false;

  return base::Base64UrlDecode(
      value, base::Base64UrlDecodePolicy::IGNORE_PADDING, salt);
}

// Parses the record size in |value| and writes the value to |*rs|. The value
// must be a positive decimal integer greater than one that does not start
// with a plus. Returns whether the record size was valid.
bool RecordSizeToInt(base::StringPiece value, uint64_t* rs) {
  if (value.empty())
    return false;

  // Reject a leading plus, as the fact that the value must be positive is
  // dictated by the specification.
  if (value[0] == '+')
    return false;

  uint64_t candidate_rs;
  if (!base::StringToUint64(value, &candidate_rs))
    return false;

  // The record size MUST be greater than one byte.
  if (candidate_rs <= 1)
    return false;

  *rs = candidate_rs;
  return true;
}

}  // namespace

EncryptionHeaderIterator::EncryptionHeaderIterator(
    std::string::const_iterator header_begin,
    std::string::const_iterator header_end)
    : iterator_(header_begin, header_end, ','),
      rs_(kDefaultRecordSizeBytes) {}

EncryptionHeaderIterator::~EncryptionHeaderIterator() {}

bool EncryptionHeaderIterator::GetNext() {
  keyid_.clear();
  salt_.clear();
  rs_ = kDefaultRecordSizeBytes;

  if (!iterator_.GetNext())
    return false;

  net::HttpUtil::NameValuePairsIterator name_value_pairs(
      iterator_.value_begin(), iterator_.value_end(), ';',
      net::HttpUtil::NameValuePairsIterator::Values::REQUIRED,
      net::HttpUtil::NameValuePairsIterator::Quotes::NOT_STRICT);

  bool found_keyid = false;
  bool found_salt = false;
  bool found_rs = false;

  while (name_value_pairs.GetNext()) {
    const base::StringPiece name = name_value_pairs.name_piece();
    const base::StringPiece value = name_value_pairs.value_piece();

    if (base::LowerCaseEqualsASCII(name, "keyid")) {
      if (found_keyid)
        return false;
      value.CopyToString(&keyid_);
      found_keyid = true;
    } else if (base::LowerCaseEqualsASCII(name, "salt")) {
      if (found_salt || !ValueToDecodedString(value, &salt_))
        return false;
      found_salt = true;
    } else if (base::LowerCaseEqualsASCII(name, "rs")) {
      if (found_rs || !RecordSizeToInt(value, &rs_))
        return false;
      found_rs = true;
    } else {
      // Silently ignore unknown directives for forward compatibility.
    }
  }

  return name_value_pairs.valid();
}

CryptoKeyHeaderIterator::CryptoKeyHeaderIterator(
    std::string::const_iterator header_begin,
    std::string::const_iterator header_end)
    : iterator_(header_begin, header_end, ',') {}

CryptoKeyHeaderIterator::~CryptoKeyHeaderIterator() {}

bool CryptoKeyHeaderIterator::GetNext() {
  keyid_.clear();
  aesgcm128_.clear();
  dh_.clear();

  if (!iterator_.GetNext())
    return false;

  net::HttpUtil::NameValuePairsIterator name_value_pairs(
      iterator_.value_begin(), iterator_.value_end(), ';',
      net::HttpUtil::NameValuePairsIterator::Values::REQUIRED,
      net::HttpUtil::NameValuePairsIterator::Quotes::NOT_STRICT);

  bool found_keyid = false;
  bool found_aesgcm128 = false;
  bool found_dh = false;

  while (name_value_pairs.GetNext()) {
    const base::StringPiece name = name_value_pairs.name_piece();
    const base::StringPiece value = name_value_pairs.value_piece();

    if (base::LowerCaseEqualsASCII(name, "keyid")) {
      if (found_keyid)
        return false;
      value.CopyToString(&keyid_);
      found_keyid = true;
    } else if (base::LowerCaseEqualsASCII(name, "aesgcm128")) {
      if (found_aesgcm128 || !ValueToDecodedString(value, &aesgcm128_))
        return false;
      found_aesgcm128 = true;
    } else if (base::LowerCaseEqualsASCII(name, "dh")) {
      if (found_dh || !ValueToDecodedString(value, &dh_))
        return false;
      found_dh = true;
    } else {
      // Silently ignore unknown directives for forward compatibility.
    }
  }

  return name_value_pairs.valid();
}

}  // namespace gcm
