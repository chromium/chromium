// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_utils.h"

#include <optional>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/family_link_user_log_record.h"
#include "components/supervised_user/core/browser/proto/parent_access_callback.pb.h"
#include "components/supervised_user/core/browser/proto/transaction_data.pb.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
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

// Parent Access widget constants, used in the local web approval
// flow.
// URL hosting the Parent Access widget.
constexpr char kParentAccessBaseURL[] =
    "https://families.google.com/parentaccess";
// URL to which the Parent Access widget redirects on approval.
constexpr char kParentAccessContinueURL[] = "https://families.google.com";
// Caller Ids for Desktop and iOS platforms.
constexpr char kParentAccessIOSCallerID[] = "qSTnVRdQ";
constexpr char kParentAccessDesktopCallerID[] = "clwAA5XJ";
constexpr char kPacpUrlPayloadMessageType[] =
    "type.googleapis.com/"
    "kids.platform.parentaccess.ui.common.proto.LocalApprovalPayload";

// A templated function to merge multiple values of the same type into either:
// * An empty optional if none of the values are set
// * A non-empty optional if all the set values are equal
// * An optional containing |mixed_value| if there are multiple different
// values.
template <class T>
std::optional<T> GetMergedRecord(const std::vector<std::optional<T>> records,
                                 T mixed_value) {
  std::optional<T> merged_record;
  for (const std::optional<T> record : records) {
    if (!record.has_value()) {
      continue;
    }

    if (merged_record.has_value() && merged_record.value() != record.value()) {
      return mixed_value;
    }
    merged_record = record;
  }
  return merged_record;
}

bool HasSupervisedStatus(
    std::optional<FamilyLinkUserLogRecord::Segment> segment) {
  if (!segment.has_value()) {
    return false;
  }
  switch (segment.value()) {
    case FamilyLinkUserLogRecord::Segment::kUnsupervised:
    case FamilyLinkUserLogRecord::Segment::kParent:
      return false;
    case FamilyLinkUserLogRecord::Segment::kSupervisionEnabledByPolicy:
    case FamilyLinkUserLogRecord::Segment::kSupervisionEnabledByUser:
      return true;
    case FamilyLinkUserLogRecord::Segment::kMixedProfile:
      NOTREACHED();
  }
}

std::optional<FamilyLinkUserLogRecord::Segment> GetLogSegmentForHistogram(
    const std::vector<FamilyLinkUserLogRecord>& records) {
  bool has_supervised_status = false;
  std::optional<FamilyLinkUserLogRecord::Segment> merged_log_segment;
  for (const FamilyLinkUserLogRecord& record : records) {
    std::optional<FamilyLinkUserLogRecord::Segment> supervision_status =
        record.GetSupervisionStatusForPrimaryAccount();
    has_supervised_status |= HasSupervisedStatus(supervision_status);
    if (merged_log_segment.has_value() &&
        merged_log_segment.value() != supervision_status) {
      if (has_supervised_status) {
        // A Family Link user record is only expected to be mixed if there is at
        // least one supervised user.
        return FamilyLinkUserLogRecord::Segment::kMixedProfile;
      }
      CHECK(merged_log_segment.value() ==
                FamilyLinkUserLogRecord::Segment::kParent ||
            merged_log_segment.value() ==
                FamilyLinkUserLogRecord::Segment::kUnsupervised);
      merged_log_segment = FamilyLinkUserLogRecord::Segment::kParent;
    } else {
      merged_log_segment = supervision_status;
    }
  }
  return merged_log_segment;
}

std::optional<WebFilterType> GetWebFilterForHistogram(
    const std::vector<FamilyLinkUserLogRecord>& records) {
  std::vector<std::optional<WebFilterType>> filter_types;
  for (const FamilyLinkUserLogRecord& record : records) {
    filter_types.push_back(record.GetWebFilterTypeForPrimaryAccount());
  }
  return GetMergedRecord(filter_types, WebFilterType::kMixed);
}

std::optional<ToggleState> GetPermissionsToggleStateForHistogram(
    const std::vector<FamilyLinkUserLogRecord>& records) {
  std::vector<std::optional<ToggleState>> permissions_toggle_states;
  for (const FamilyLinkUserLogRecord& record : records) {
    permissions_toggle_states.push_back(
        record.GetPermissionsToggleStateForPrimaryAccount());
  }
  return GetMergedRecord(permissions_toggle_states, ToggleState::kMixed);
}

std::optional<ToggleState> GetExtensionsToggleStateForHistogram(
    const std::vector<FamilyLinkUserLogRecord>& records) {
  std::vector<std::optional<ToggleState>> extensions_toggle_states;
  for (const FamilyLinkUserLogRecord& record : records) {
    extensions_toggle_states.push_back(
        record.GetExtensionsToggleStateForPrimaryAccount());
  }
  return GetMergedRecord(extensions_toggle_states, ToggleState::kMixed);
}

// Returns the text that will be shown as the PACP widget subtitle, containing
// information about the blocked hostname and the blocking reason.
std::string GetBlockingReasonSubtitle(
    const std::u16string blocked_hostname,
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
    const std::u16string blocked_hostname,
    supervised_user::FilteringBehaviorReason filtering_reason) {
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
  CHECK(base_64_url_encoded_data.length() > 0);
  return base_64_url_encoded_data;
}

// Returns the PACP widget url with the appropriate query parameters.
GURL GetParentAccessURL(
    const std::string& caller_id,
    const std::string& locale,
    std::optional<GURL> blocked_url,
    supervised_user::FilteringBehaviorReason filtering_reason) {
  GURL url(kParentAccessBaseURL);
  GURL::Replacements replacements;
  std::string query = base::StrCat({"callerid=", caller_id, "&hl=", locale,
                                    "&continue=", kParentAccessContinueURL});
  if (base::FeatureList::IsEnabled(
          kLocalWebApprovalsWidgetSupportsUrlPayload) &&
      blocked_url.has_value() && !blocked_url.value().host().empty()) {
    // Prepare blocked URL hostname for user-friendly display, including internationalized
    // domain name (IDN) conversion if necessary.
    std::u16string blocked_hostname = url_formatter::FormatUrl(
        blocked_url.value(),
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

  if (!url.host().starts_with(kPacpOriginUrlHost) ||
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

bool EmitLogRecordHistograms(
    const std::vector<FamilyLinkUserLogRecord>& records) {
  bool did_emit_histogram = false;

  std::optional<FamilyLinkUserLogRecord::Segment> segment =
      GetLogSegmentForHistogram(records);
  if (segment.has_value()) {
    base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                  segment.value());
    did_emit_histogram = true;
  }

  std::optional<WebFilterType> web_filter = GetWebFilterForHistogram(records);
  if (web_filter.has_value()) {
    base::UmaHistogramEnumeration(
        kFamilyLinkUserLogSegmentWebFilterHistogramName, web_filter.value());
    did_emit_histogram = true;
  }

  std::optional<ToggleState> permissions_toggle_state =
      GetPermissionsToggleStateForHistogram(records);
  if (permissions_toggle_state.has_value()) {
    base::UmaHistogramEnumeration(
        kSitesMayRequestCameraMicLocationHistogramName,
        permissions_toggle_state.value());
    did_emit_histogram = true;
  }

  std::optional<ToggleState> extensions_toggle_state =
      GetExtensionsToggleStateForHistogram(records);
  if (extensions_toggle_state.has_value()) {
    base::UmaHistogramEnumeration(
        kSkipParentApprovalToInstallExtensionsHistogramName,
        extensions_toggle_state.value());
    did_emit_histogram = true;
  }

  return did_emit_histogram;
}

UrlFormatter::UrlFormatter(
    const SupervisedUserURLFilter& supervised_user_url_filter,
    FilteringBehaviorReason filtering_behavior_reason)
    : supervised_user_url_filter_(supervised_user_url_filter),
      filtering_behavior_reason_(filtering_behavior_reason) {}

UrlFormatter::~UrlFormatter() = default;

GURL UrlFormatter::FormatUrl(const GURL& url) const {
  // Strip the trivial subdomain.
  GURL stripped_url(url_formatter::FormatUrl(
      url, url_formatter::kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));

  // If the url is blocked due to an entry in the block list,
  // check if the blocklist entry is a trivial www-subdomain conflict and skip
  // the stripping.
  bool skip_trivial_subdomain_strip =
      filtering_behavior_reason_ == FilteringBehaviorReason::MANUAL &&
      stripped_url.host() != url.host() &&
      supervised_user_url_filter_->IsHostInBlocklist(url.host());

  GURL target_url = skip_trivial_subdomain_strip ? url : stripped_url;

  // TODO(b/322484529): Standardize the url formatting for local approvals
  // across platforms.
#if !BUILDFLAG(IS_CHROMEOS)
  return NormalizeUrl(target_url);
#else
  return target_url;
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

GURL GetParentAccessURLForIOS(
    const std::string& locale,
    const GURL& blocked_url,
    supervised_user::FilteringBehaviorReason filtering_reason) {
  return GetParentAccessURL(kParentAccessIOSCallerID, locale, blocked_url,
                            filtering_reason);
}

GURL GetParentAccessURLForDesktop(
    const std::string& locale,
    const GURL& blocked_url,
    supervised_user::FilteringBehaviorReason filtering_reason) {
  return GetParentAccessURL(kParentAccessDesktopCallerID, locale, blocked_url,
                            filtering_reason);
}

}  // namespace supervised_user
