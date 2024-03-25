// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/local_credential_management.h"

#include <string_view>

#include "base/i18n/string_compare.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

CredentialComparator::CredentialComparator() {
  UErrorCode error = U_ZERO_ERROR;
  collator_.reset(
      icu::Collator::createInstance(icu::Locale::getDefault(), error));
}

CredentialComparator::~CredentialComparator() = default;

bool CredentialComparator::operator()(
    const device::DiscoverableCredentialMetadata& a,
    const device::DiscoverableCredentialMetadata& b) {
  UCollationResult relation = base::i18n::CompareString16WithCollator(
      *collator_, ETLDPlus1(a.rp_id), ETLDPlus1(b.rp_id));
  if (relation != UCOL_EQUAL) {
    return relation == UCOL_LESS;
  }

  relation = base::i18n::CompareString16WithCollator(
      *collator_, LabelReverse(a.rp_id), LabelReverse(b.rp_id));
  if (relation != UCOL_EQUAL) {
    return relation == UCOL_LESS;
  }

  relation = base::i18n::CompareString16WithCollator(
      *collator_, base::UTF8ToUTF16(a.user.name.value_or("")),
      base::UTF8ToUTF16(b.user.name.value_or("")));
  if (relation != UCOL_EQUAL) {
    return relation == UCOL_LESS;
  }
  return a.cred_id < b.cred_id;
}

std::u16string CredentialComparator::ETLDPlus1(const std::string& rp_id) {
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      rp_id, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty()) {
    domain = rp_id;
  }
  return base::UTF8ToUTF16(domain);
}

std::u16string CredentialComparator::LabelReverse(const std::string& rp_id) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      rp_id, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::reverse(parts.begin(), parts.end());
  return base::UTF8ToUTF16(base::JoinString(parts, "."));
}
