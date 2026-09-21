// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_sender_domain_matcher.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/affiliations/core/browser/match_type.h"
#include "components/url_formatter/url_formatter.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace one_time_tokens {

namespace {

std::string ExtractEmailDomain(std::string_view email) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      email, "@", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() == 2) {
    return std::string(parts[1]);
  }
  return std::string();
}

}  // namespace

std::ostream& operator<<(std::ostream& os,
                         GmailOtpSenderDomainMatchType match_type) {
  switch (match_type) {
    case GmailOtpSenderDomainMatchType::kUnknown:
      return os << "kUnknown";
    case GmailOtpSenderDomainMatchType::kNoMatch:
      return os << "kNoMatch";
    case GmailOtpSenderDomainMatchType::kGrouped:
      return os << "kGrouped";
    case GmailOtpSenderDomainMatchType::kPsl:
      return os << "kPsl";
    case GmailOtpSenderDomainMatchType::kGroupedAndPsl:
      return os << "kGroupedAndPsl";
    case GmailOtpSenderDomainMatchType::kExact:
      return os << "kExact";
    case GmailOtpSenderDomainMatchType::kAffiliated:
      return os << "kAffiliated";
    case GmailOtpSenderDomainMatchType::kFrameIsWwwPsl:
      return os << "kFrameIsWwwPsl";
  }
  return os << static_cast<int>(match_type);
}

GmailOtpSenderDomainMatcher::GmailOtpSenderDomainMatcher(
    std::unique_ptr<affiliations::DomainRelationChecker>
        domain_relation_checker,
    const url::Origin& otp_frame_origin)
    : domain_relation_checker_(std::move(domain_relation_checker)),
      otp_frame_origin_(otp_frame_origin) {
  CHECK(domain_relation_checker_);
}

GmailOtpSenderDomainMatcher::~GmailOtpSenderDomainMatcher() = default;

void GmailOtpSenderDomainMatcher::Check(std::string_view sender_address,
                                        ResultCallback callback) {
  CHECK(callback);
  CHECK(!otp_frame_origin_.opaque());

  std::string sender_domain = ExtractEmailDomain(sender_address);

  url::SchemeHostPort frame_tuple =
      otp_frame_origin_.GetTupleOrPrecursorTupleIfOpaque();
  url::SchemeHostPort sender_tuple(
      url::kHttpsScheme, std::move(sender_domain),
      url::DefaultPortForScheme(url::kHttpsScheme));

  // `domain_relation_checker_` is destroyed together with `this`, which already
  // cancels the check. The weak pointer makes that explicit and keeps the
  // cancellation independent of the checker's implementation.
  domain_relation_checker_->Check(
      frame_tuple, sender_tuple,
      base::BindOnce(&GmailOtpSenderDomainMatcher::OnDomainRelationChecked,
                     weak_ptr_factory_.GetWeakPtr(), sender_tuple,
                     std::move(callback)));
}

void GmailOtpSenderDomainMatcher::OnDomainRelationChecked(
    const url::SchemeHostPort& sender_tuple,
    ResultCallback callback,
    std::optional<affiliations::MatchType> match_type) {
  std::move(callback).Run(ResolveMatchType(match_type, sender_tuple));
}

GmailOtpSenderDomainMatchType GmailOtpSenderDomainMatcher::ResolveMatchType(
    std::optional<affiliations::MatchType> match_type,
    const url::SchemeHostPort& sender_tuple) const {
  if (!match_type.has_value()) {
    return GmailOtpSenderDomainMatchType::kNoMatch;
  }
  if (*match_type == affiliations::MatchType::kExact) {
    return GmailOtpSenderDomainMatchType::kExact;
  }

  int value = static_cast<int>(*match_type);
  bool has_psl = value & static_cast<int>(affiliations::MatchType::kPSL);
  bool has_grouped =
      value & static_cast<int>(affiliations::MatchType::kGrouped);
  bool has_affiliated =
      value & static_cast<int>(affiliations::MatchType::kAffiliated);

  if (has_psl) {
    std::string stripped_frame_host =
        url_formatter::StripWWW(otp_frame_origin_.host());
    url::SchemeHostPort stripped_frame_tuple(otp_frame_origin_.scheme(),
                                             std::move(stripped_frame_host),
                                             otp_frame_origin_.port());
    if (stripped_frame_tuple == sender_tuple) {
      return GmailOtpSenderDomainMatchType::kFrameIsWwwPsl;
    }
  }

  if (has_affiliated) {
    return GmailOtpSenderDomainMatchType::kAffiliated;
  }
  if (has_grouped && has_psl) {
    return GmailOtpSenderDomainMatchType::kGroupedAndPsl;
  }
  if (has_psl) {
    return GmailOtpSenderDomainMatchType::kPsl;
  }
  if (has_grouped) {
    return GmailOtpSenderDomainMatchType::kGrouped;
  }
  return GmailOtpSenderDomainMatchType::kUnknown;
}

}  // namespace one_time_tokens
