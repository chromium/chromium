// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_UTILS_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "components/safe_search_api/url_checker.h"
#include "components/supervised_user/core/browser/proto/families_common.pb.h"
#include "components/supervised_user/core/browser/proto/parent_access_callback.pb.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "url/gurl.h"

namespace supervised_user {

// Reason for applying the website filtering parental control.
enum class FilteringBehaviorReason {
  DEFAULT = 0,
  ASYNC_CHECKER = 1,
  MANUAL = 2,
  FILTER_DISABLED = 3,
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

// Declaration for gtest: defining in prod code is not required.
void PrintTo(FilteringBehavior behavior, std::ostream* os);

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

// Returns the right URL of the PACP widget for iOS and Desktop platforms (other
// platforms are undefined).
// `locale` is the display language (go/bcp47).
// `blocked_url` is the url subject to approval that is shown in the PACP
// widget.
// `filtering_reason` is the reason for blocking the url, which is reflected
// in the subtitle of the PACP widget.
GURL GetParentAccessURL(
    const std::string& locale,
    const GURL& blocked_url,
    supervised_user::FilteringBehaviorReason filtering_reason);
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_UTILS_H_
