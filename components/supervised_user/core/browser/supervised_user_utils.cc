// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_utils.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/proto/parent_access_callback.pb.h"
#include "components/supervised_user/core/browser/proto/transaction_data.pb.h"
#include "components/supervised_user/core/browser/supervised_user_log_record.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/url_formatter/url_formatter.h"
#include "components/url_matcher/url_util.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace supervised_user {

namespace {

// Url query param returned from the PACP widget with an encoded parent approval
// result.
constexpr char kParentAccessResultQueryParameter[] = "result";

// Url that contains the approval result in PACP parent approval requests.
constexpr char kPacpOriginUrlHost[] = "families.google.com";

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
// Returns the text that will be shown as the PACP widget subtitle, containing
// information about the blocked hostname and the blocking reason.
std::string GetBlockingReasonSubtitle(
    const std::u16string& blocked_hostname,
    supervised_user::FilteringBehaviorReason filtering_reason) {
  int message_id = 0;
  switch (filtering_reason) {
    case FilteringBehaviorReason::DEFAULT:
      message_id = IDS_PARENT_WEBSITE_APPROVAL_BLOCK_ALL_URL;
      break;
    case FilteringBehaviorReason::ASYNC_CHECKER:
      message_id = IDS_PARENT_WEBSITE_APPROVAL_SAFE_SITES_URL;
      break;
    case FilteringBehaviorReason::MANUAL:
      message_id = IDS_PARENT_WEBSITE_APPROVAL_MANUAL_URL;
      break;
    case FilteringBehaviorReason::FILTER_DISABLED:
      NOTREACHED() << "No blocking reason when the filter is disabled";
  }
  return l10n_util::GetStringFUTF8(message_id, blocked_hostname);
}

// Returns a base64-encoded `TransactionData` proto message that encapsulates
// the blocked url so that PACP can consume it.
std::string GetBase64EncodedInTransactionalDataForPayload(
    const std::u16string& blocked_hostname,
    supervised_user::FilteringBehaviorReason filtering_reason) {
  static constexpr char kPacpUrlPayloadMessageType[] =
      "type.googleapis.com/"
      "kids.platform.parentaccess.ui.common.proto.LocalApprovalPayload";

  CHECK(!blocked_hostname.empty());
  kids::platform::parentaccess::proto::LocalApprovalPayload approval_url;
  approval_url.set_url_approval_context(
      GetBlockingReasonSubtitle(blocked_hostname, filtering_reason));

  kids::platform::parentaccess::proto::TransactionData transaction_data;
  transaction_data.mutable_payload()->set_value(
      approval_url.SerializeAsString());
  transaction_data.mutable_payload()->set_type_url(kPacpUrlPayloadMessageType);
  std::string base_64_url_encoded_data;
  base::Base64UrlEncode(transaction_data.SerializeAsString(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &base_64_url_encoded_data);
  CHECK_GT(base_64_url_encoded_data.length(), 0UL);
  return base_64_url_encoded_data;
}
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
}  // namespace

ParentAccessCallbackParsedResult::ParentAccessCallbackParsedResult(
    ParentAccessWidgetError error)
    : result_(error) {}
ParentAccessCallbackParsedResult::ParentAccessCallbackParsedResult(
    kids::platform::parentaccess::client::proto::ParentAccessCallback callback)
    : result_(callback) {}
ParentAccessCallbackParsedResult::~ParentAccessCallbackParsedResult() = default;

std::optional<ParentAccessWidgetError>
ParentAccessCallbackParsedResult::GetError() const {
  if (std::holds_alternative<ParentAccessWidgetError>(result_)) {
    return std::get<ParentAccessWidgetError>(result_);
  }
  return std::nullopt;
}

std::optional<kids::platform::parentaccess::client::proto::ParentAccessCallback>
ParentAccessCallbackParsedResult::GetCallback() const {
  if (std::holds_alternative<
          kids::platform::parentaccess::client::proto::ParentAccessCallback>(
          result_)) {
    return std::get<
        kids::platform::parentaccess::client::proto::ParentAccessCallback>(
        result_);
  }
  return std::nullopt;
}

// static
ParentAccessCallbackParsedResult
ParentAccessCallbackParsedResult::ParseParentAccessCallbackResult(
    const std::string& encoded_parent_access_callback_proto) {
  std::string decoded_parent_access_callback;
  if (!base::Base64UrlDecode(encoded_parent_access_callback_proto,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &decoded_parent_access_callback)) {
    LOG(ERROR) << "ParentAccessHandler::ParentAccessResult: Error decoding "
                  "parent_access_result from base64";
    return ParentAccessCallbackParsedResult(
        ParentAccessWidgetError::kDecodingError);
  }

  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  if (!parent_access_callback.ParseFromString(decoded_parent_access_callback)) {
    LOG(ERROR) << "ParentAccessHandler::ParentAccessResult: Error parsing "
                  "decoded_parent_access_result to proto";
    return ParentAccessCallbackParsedResult(
        ParentAccessWidgetError::kParsingError);
  }

  return ParentAccessCallbackParsedResult(parent_access_callback);
}

std::optional<std::string> MaybeGetPacpResultFromUrl(const GURL& url) {
  std::string result_value;
  bool contains_result_query_param = net::GetValueForKeyInQuery(
      url,
      /*search_key=*/kParentAccessResultQueryParameter,
      /*out_value=*/&result_value);

  if (!url.GetHost().starts_with(kPacpOriginUrlHost) ||
      !contains_result_query_param) {
    return std::nullopt;
  }
  return result_value;
}

std::string FamilyRoleToString(kidsmanagement::FamilyRole role) {
  switch (role) {
    case kidsmanagement::CHILD:
      return "child";
    case kidsmanagement::MEMBER:
      return "member";
    case kidsmanagement::PARENT:
      return "parent";
    case kidsmanagement::HEAD_OF_HOUSEHOLD:
      return "family_manager";
    default:
      // Keep the previous semantics - other values were not allowed.
      NOTREACHED();
  }
}

GURL NormalizeUrl(const GURL& url) {
  GURL effective_url = url_matcher::util::GetEmbeddedURL(url);
  if (!effective_url.is_valid()) {
    effective_url = url;
  }
  return url_matcher::util::Normalize(effective_url);
}

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
GURL GetParentAccessURL(
    const std::string& locale,
    const GURL& blocked_url,
    supervised_user::FilteringBehaviorReason filtering_reason) {
  // Parent Access widget constants, used in the local web approval
  // flow.
  // URL hosting the Parent Access widget.
  static constexpr char kBaseUrl[] = "https://families.google.com/parentaccess";
  // URL to which the Parent Access widget redirects on approval.
  static constexpr char kContinueUrl[] = "https://families.google.com";

  // Caller Ids for Desktop and iOS platforms.
#if BUILDFLAG(IS_IOS)
  static constexpr char kCallerId[] = "qSTnVRdQ";
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  static constexpr char kCallerId[] = "clwAA5XJ";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

  GURL url(kBaseUrl);
  GURL::Replacements replacements;
  std::string query = base::StrCat(
      {"callerid=", kCallerId, "&hl=", locale, "&continue=", kContinueUrl});
  if (base::FeatureList::IsEnabled(
          kLocalWebApprovalsWidgetSupportsUrlPayload) &&
      !blocked_url.GetHost().empty()) {
    // Prepare blocked URL hostname for user-friendly display, including
    // internationalized domain name (IDN) conversion if necessary.
    std::u16string blocked_hostname = url_formatter::FormatUrl(
        blocked_url,
        url_formatter::kFormatUrlOmitHTTP | url_formatter::kFormatUrlOmitHTTPS |
            url_formatter::kFormatUrlOmitDefaults,
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
    query += base::StrCat(
        {"&transaction-data=", GetBase64EncodedInTransactionalDataForPayload(
                                   blocked_hostname, filtering_reason)});
  }
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
}  // namespace supervised_user
