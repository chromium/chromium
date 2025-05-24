// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_UTILS_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/memory/raw_ref.h"
#include "components/safe_search_api/url_checker.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/supervised_user/core/browser/family_link_user_log_record.h"
#include "components/supervised_user/core/browser/proto/families_common.pb.h"
#include "components/supervised_user/core/browser/proto/parent_access_callback.pb.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "url/gurl.h"

class GURL;

namespace supervised_user {

// Reason for applying the website filtering parental control.
enum class FilteringBehaviorReason {
  DEFAULT = 0,
  ASYNC_CHECKER = 1,
  MANUAL = 2,
  FILTER_DISABLED = 3,
};

// Details degarding how a particular filtering classification was arrived at.
struct FilteringBehaviorDetails {
  FilteringBehaviorReason reason;

  // The following field only applies if `reason` is `ASYNC_CHECKER`.
  safe_search_api::ClassificationDetails classification_details;
};

// A Java counterpart will be generated for this enum.
// Values are stored in prefs under kDefaultSupervisedUserFilteringBehavior.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.superviseduser
enum class FilteringBehavior : int {
  kAllow = 0,
  // Deprecated, kWarn = 1.
  kBlock = 2,
  kInvalid = 3,
};

// Whether the migration of existing extensions to parent-approved needs to be
// executed, when the feature
// `kEnableSupervisedUserSkipParentApprovalToInstallExtensions` becomes enabled.
enum class LocallyParentApprovedExtensionsMigrationState : int {
  kNeedToRun = 0,
  kComplete = 1,
};

// Wrapper for the different outcomes: holds either an error or a valid
// parsed PACP callback result that can be returned by the PACP widget.
class ParentAccessCallbackParsedResult {
 public:
  explicit ParentAccessCallbackParsedResult(ParentAccessWidgetError error);
  explicit ParentAccessCallbackParsedResult(
      kids::platform::parentaccess::client::proto::ParentAccessCallback
          callback);

  ParentAccessCallbackParsedResult() = delete;
  ParentAccessCallbackParsedResult(ParentAccessCallbackParsedResult&) = delete;
  ParentAccessCallbackParsedResult& operator=(
      const ParentAccessCallbackParsedResult&) = delete;
  ~ParentAccessCallbackParsedResult();

  std::optional<ParentAccessWidgetError> GetError() const;
  std::optional<
      kids::platform::parentaccess::client::proto::ParentAccessCallback>
  GetCallback() const;

  // Decodes and parses the the base64 result provided by the PACP widget.
  // See https://tools.ietf.org/html/rfc4648#section-5.
  static ParentAccessCallbackParsedResult ParseParentAccessCallbackResult(
      const std::string& encoded_parent_access_callback_proto);

 private:
  std::variant<
      kids::platform::parentaccess::client::proto::ParentAccessCallback,
      ParentAccessWidgetError>
      result_;
};

// Extracts a parent approval result from a url query parameter returned by the
// PACP widget, if the provided url must contain a `result=` query param.
// If not such query param value exists the method returns an empty optional.
std::optional<std::string> MaybeGetPacpResultFromUrl(const GURL& url);

// Converts FamilyRole enum to string format.
std::string FamilyRoleToString(kidsmanagement::FamilyRole role);

// Strips user-specific tokens in a URL to generalize it.
GURL NormalizeUrl(const GURL& url);

// Given a list of records that map to the supervision state of primary
// accounts on the user's device, emits metrics that reflect the FamilyLink
// settings of the user.
// Returns true if one or more histograms were emitted.
bool EmitLogRecordHistograms(
    const std::vector<FamilyLinkUserLogRecord>& records);

// Url formatter helper.
// Decisions on how to format the url depend on the filtering reason,
// the manual parental url block-list.
class UrlFormatter {
 public:
  UrlFormatter(const SupervisedUserURLFilter& supervised_user_url_filter,
               FilteringBehaviorReason filtering_behavior_reason);
  ~UrlFormatter();
  GURL FormatUrl(const GURL& url) const;

 private:
  const raw_ref<const SupervisedUserURLFilter> supervised_user_url_filter_;
  const FilteringBehaviorReason filtering_behavior_reason_;
};

// Returns the URL of the PACP widget for the iOS local web approval flow.
// `locale` is the display language (go/bcp47).
// `blocked_url` is the url subject to approval that is shown in the PACP
// widget.
// `filtering_reason` is the reason for blocking the url, which is reflected
// in the subtitle of the PACP widget.
GURL GetParentAccessURLForIOS(
    const std::string& locale,
    const GURL& blocked_url,
    supervised_user::FilteringBehaviorReason filtering_reason);

// Returns the URL of the PACP widget for the Desktop local web approval flow.
// `locale` is the display language (go/bcp47).
// `blocked_url` is the url subject to approval that is shown in the PACP
// widget.
// `filtering_reason` is the reason for blocking the url, which is reflected
// in the subtitle of the PACP widget.
GURL GetParentAccessURLForDesktop(
    const std::string& locale,
    const GURL& blocked_url,
    supervised_user::FilteringBehaviorReason filtering_reason);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_UTILS_H_
