// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/test_state_seeded_observer.h"

#include <memory>
#include <optional>
#include <ostream>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/supervised_user/family_member.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace supervised_user {

std::ostream& operator<<(std::ostream& os, const FilteringBehavior& fb) {
  switch (fb) {
    case FilteringBehavior::kAllow:
      os << "kAllow";
      break;
    case FilteringBehavior::kBlock:
      os << "kBlock";
      break;
    case FilteringBehavior::kInvalid:
      os << "kInvalid";
      break;
  }
  return os;
}

namespace {

std::string GetToggleAbbrev(FamilyLinkToggleType toggle) {
  switch (toggle) {
    case FamilyLinkToggleType::kPermissionsToggle:
      return "PERMISSIONS";
    case FamilyLinkToggleType::kExtensionsToggle:
      return "EXTENSIONS";
    case FamilyLinkToggleType::kCookiesToggle:
      return "COOKIES";
    default:
      NOTREACHED_NORETURN();
  }
}

net::NetworkTrafficAnnotationTag TestStateSeedTag() {
  return net::DefineNetworkTrafficAnnotation(
      "supervised_user_test_state_seeding",
      R"(
semantics {
  sender: "Supervised Users"
  description:
    "Seeds test state for end-to-end tests of supervision features in behalf of
test accounts." trigger: "Execution of end-to-end tests." data: "An OAuth2
access token identifying and authenticating the " "Google account, and the
subject of seeding identified by Gaia Id." destination: GOOGLE_OWNED_SERVICE
  internal {
    contacts {
      email: "chrome-kids-eng@google.com"
    }
  }
  user_data {
    type: NONE
  }
  last_reviewed: "2023-12-20"
}
policy {
  cookies_allowed: NO
  setting:
    "This does not apply to real users and can't be disabled."
  policy_exception_justification:
    "Feature is not intended to work with real user accounts."
  })");
}

constexpr FetcherConfig kDefineChromeTestStateConfig{
    .service_path = FetcherConfig::PathTemplate(
        "/kidsmanagement/v1/people/{}/websites:defineChromeTestState"),
    .method = FetcherConfig::Method::kPost,
    .traffic_annotation = TestStateSeedTag,
    .access_token_config =
        {
            .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
            // TODO(b/284523446): Refer to GaiaConstants rather than literal.
            .oauth2_scope = "https://www.googleapis.com/auth/kid.permission",
        },
    .request_priority = net::IDLE,
};

constexpr FetcherConfig kResetChromeTestStateConfig{
    .service_path = FetcherConfig::PathTemplate(
        "/kidsmanagement/v1/people/{}/websites:resetChromeTestState"),
    .method = FetcherConfig::Method::kPost,
    .traffic_annotation = TestStateSeedTag,
    .access_token_config =
        {
            .mode = signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
            // TODO(b/284523446): Refer to GaiaConstants rather than
            // literal.
            .oauth2_scope = "https://www.googleapis.com/auth/kid.permission",
        },
    .request_priority = net::IDLE,
};

// Helper method that extends DefineChromeTestStateRequest proto with an
// instance of WebsiteException.
inline void AddWebsiteException(
    kidsmanagement::DefineChromeTestStateRequest& request,
    const GURL& url,
    kidsmanagement::ExceptionType exception_type) {
  kidsmanagement::WebsiteException* exception =
      request.mutable_url_filtering_settings()->add_exceptions();
  // DefineChromeTestStateRequest requires patterns rather than fully-qualified
  // urls. Host part works well in this case.
  exception->set_pattern(url.host());
  exception->set_exception_type(exception_type);
}

void WaitForRequestToComplete(const FamilyMember& supervising_user,
                              const FamilyMember& browser_user,
                              const FetcherConfig& config,
                              std::string_view serialized_request) {
  // Start fetching and wait for the response.
  base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
  StatusFetcher fetcher(
      *supervising_user.identity_manager(),
      supervising_user.url_loader_factory(), serialized_request, config,
      {browser_user.GetAccountId().ToString()},
      base::BindLambdaForTesting([&](const ProtoFetcherStatus& status) {
        CHECK(status.IsOk()) << "WaitForRequestToComplete failed";
        run_loop.Quit();
      }));
  run_loop.Run();
}

bool AreSafeSitesConfigured(const FamilyMember& member) {
  PrefService* pref_service = member.browser()->profile()->GetPrefs();
  CHECK(pref_service);

  if (!IsSafeSitesEnabled(*pref_service)) {
    return false;
  }

  SupervisedUserURLFilter* url_filter =
      member.supervised_user_service()->GetURLFilter();
  CHECK(url_filter);

  return url_filter->GetDefaultFilteringBehavior() == FilteringBehavior::kAllow;
}

bool IsUrlConfigured(SupervisedUserURLFilter& url_filter,
                     const GURL& url,
                     FilteringBehavior expected_filtering_behavior) {
  FilteringBehavior filtering_behavior;
  if (!url_filter.GetManualFilteringBehaviorForURL(url, &filtering_behavior)) {
    // The change that arrived doesn't have the manual mode for requested url
    // - wait for the next one.
    LOG(WARNING) << "IsUrlConfigured: no manual mode for " << url.spec();
    return false;
  }

  if (filtering_behavior != expected_filtering_behavior) {
    LOG(WARNING) << "IsUrlConfigured: filtering behavior mismatch, actual="
                 << filtering_behavior
                 << " expected=" << expected_filtering_behavior;
    return false;
  }
  return true;
}

bool UrlFiltersAreConfigured(const FamilyMember& family_member,
                             const std::optional<GURL>& allowed_url,
                             const std::optional<GURL>& blocked_url) {
  SupervisedUserURLFilter* url_filter =
      family_member.supervised_user_service()->GetURLFilter();
  CHECK(url_filter);

  if (!AreSafeSitesConfigured(family_member)) {
    return false;
  }

  if (allowed_url.has_value()) {
    if (!IsUrlConfigured(*url_filter, *allowed_url,
                         FilteringBehavior::kAllow)) {
      LOG(WARNING) << allowed_url->spec()
                   << " is not configured yet (requested: kAllow).";
      return false;
    }
  }

  if (blocked_url.has_value()) {
    if (!IsUrlConfigured(*url_filter, *blocked_url,
                         FilteringBehavior::kBlock)) {
      LOG(WARNING) << blocked_url->spec()
                   << " is not configured yet (requested: kBlock).";
      return false;
    }
  }
  return true;
}

bool UrlFiltersAreEmpty(const FamilyMember& family_member) {
  return family_member.supervised_user_service()
      ->GetURLFilter()
      ->IsManualHostsEmpty();
}

bool ToggleHasExpectedValue(const FamilyMember& browser_user,
                            FamilyLinkToggleConfiguration toggle) {
  content_settings::ProviderType provider_type;
  const HostContentSettingsMap& map =
      *HostContentSettingsMapFactory::GetForProfile(
          browser_user.browser()->profile());
  PrefService& prefs = *browser_user.browser()->profile()->GetPrefs();

  if (toggle.type == FamilyLinkToggleType::kCookiesToggle) {
    bool can_block_cookies = static_cast<bool>(toggle.state);
    map.GetDefaultContentSetting(ContentSettingsType::COOKIES, &provider_type);
    // The supervised user can block the cookies if the corresponding content
    // provider is not supervised.
    return can_block_cookies ==
           (provider_type !=
            content_settings::ProviderType::kSupervisedProvider);
  }
  if (toggle.type == FamilyLinkToggleType::kPermissionsToggle) {
    bool permission_pref_has_expected_value =
        prefs.GetBoolean(
            prefs::kSupervisedUserExtensionsMayRequestPermissions) ==
        static_cast<bool>(toggle.state);

    // Note: The Family Link permissions toggle is mapped to the above
    // preference, but with the transition to the updated extension flow the
    // preference will become deprecated. The switch will still apply to other
    // features such as blocking geolocation.
    bool is_geolocation_blocked = !static_cast<bool>(toggle.state);
    // The supervised user has the geolocation blocked if the corresponding
    // content setting is blocked.
    bool is_geolocation_configured =
        is_geolocation_blocked ==
        (map.GetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                      &provider_type) ==
         ContentSetting::CONTENT_SETTING_BLOCK);

    return permission_pref_has_expected_value && is_geolocation_configured;
  }
  CHECK(toggle.type == FamilyLinkToggleType::kExtensionsToggle);
  return prefs.GetBoolean(prefs::kSkipParentApprovalToInstallExtensions) ==
         static_cast<bool>(toggle.state);
}
}  // namespace

BrowserState::~BrowserState() = default;
BrowserState::BrowserState(const Intent* intent) : intent_(intent) {}

BrowserState BrowserState::Reset() {
  return BrowserState(new ResetIntent());
}
BrowserState BrowserState::EnableSafeSites() {
  return BrowserState(new DefineManualSiteListIntent());
}
BrowserState BrowserState::AllowSite(const GURL& gurl) {
  return BrowserState(new DefineManualSiteListIntent(
      DefineManualSiteListIntent::AllowUrl(gurl)));
}
BrowserState BrowserState::BlockSite(const GURL& gurl) {
  return BrowserState(new DefineManualSiteListIntent(
      DefineManualSiteListIntent::BlockUrl(gurl)));
}
BrowserState BrowserState::AdvancedSettingsToggles(
    std::list<FamilyLinkToggleConfiguration> toggle_list) {
  return BrowserState(new ToggleIntent(std::move(toggle_list)));
}

BrowserState BrowserState::SetAdvancedSettingsDefault() {
  FamilyLinkToggleConfiguration extensions_toggle(
      {.type = FamilyLinkToggleType::kExtensionsToggle,
       .state = FamilyLinkToggleState::kDisabled});
  FamilyLinkToggleConfiguration permissions_toggle(
      {.type = FamilyLinkToggleType::kPermissionsToggle,
       .state = FamilyLinkToggleState::kEnabled});
  FamilyLinkToggleConfiguration cookies_toggle(
      {.type = FamilyLinkToggleType::kCookiesToggle,
       .state = FamilyLinkToggleState::kDisabled});
  return AdvancedSettingsToggles(
      {extensions_toggle, permissions_toggle, cookies_toggle});
}

void BrowserState::Seed(const FamilyMember& supervising_user,
                        const FamilyMember& browser_user) const {
  WaitForRequestToComplete(supervising_user, browser_user, intent_->GetConfig(),
                           intent_->GetRequest());
}

bool BrowserState::Check(const FamilyMember& browser_user) const {
  return intent_->Check(browser_user);
}

std::string BrowserState::ToString() const {
  return intent_->ToString();
}

BrowserState::Intent::~Intent() = default;

BrowserState::ResetIntent::~ResetIntent() = default;
std::string BrowserState::ResetIntent::GetRequest() const {
  return kidsmanagement::ResetChromeTestStateRequest().SerializeAsString();
}
const FetcherConfig& BrowserState::ResetIntent::GetConfig() const {
  return kResetChromeTestStateConfig;
}
std::string BrowserState::ResetIntent::ToString() const {
  return "Reset";
}
bool BrowserState::ResetIntent::Check(const FamilyMember& browser_user) const {
  bool result = UrlFiltersAreEmpty(browser_user);
  LOG(WARNING) << "BrowserState::ResetIntent = " << (result ? "true" : "false");
  return result;
}

BrowserState::DefineManualSiteListIntent::DefineManualSiteListIntent() =
    default;
BrowserState::DefineManualSiteListIntent::DefineManualSiteListIntent(
    AllowUrl url)
    : allowed_url_(url) {}
BrowserState::DefineManualSiteListIntent::DefineManualSiteListIntent(
    BlockUrl url)
    : blocked_url_(url) {}
BrowserState::DefineManualSiteListIntent::~DefineManualSiteListIntent() =
    default;

std::string BrowserState::DefineManualSiteListIntent::GetRequest() const {
  kidsmanagement::DefineChromeTestStateRequest request;
  if (allowed_url_.has_value()) {
    AddWebsiteException(request, *allowed_url_, kidsmanagement::ALLOW);
  }
  if (blocked_url_.has_value()) {
    AddWebsiteException(request, *blocked_url_, kidsmanagement::BLOCK);
  }

  request.mutable_url_filtering_settings()->set_filter_level(
      kidsmanagement::SAFE_SITES);
  return request.SerializeAsString();
}
const supervised_user::FetcherConfig&
BrowserState::DefineManualSiteListIntent::GetConfig() const {
  return kDefineChromeTestStateConfig;
}
std::string BrowserState::DefineManualSiteListIntent::ToString() const {
  std::vector<std::string> bits;
  bits.push_back("Define[SAFE_SITES");
  if (allowed_url_.has_value()) {
    bits.push_back(",allow=");
    bits.push_back(allowed_url_->spec());
  }
  if (blocked_url_.has_value()) {
    bits.push_back(",block=");
    bits.push_back(blocked_url_->spec());
  }
  bits.push_back("]");
  return base::StrCat(bits);
}
bool BrowserState::DefineManualSiteListIntent::Check(
    const FamilyMember& browser_user) const {
  bool result =
      UrlFiltersAreConfigured(browser_user, allowed_url_, blocked_url_);
  LOG(WARNING) << "BrowserState::DefineManualSiteListIntent = "
               << (result ? "true" : "false");
  return result;
}

BrowserState::ToggleIntent::ToggleIntent(
    std::list<FamilyLinkToggleConfiguration> toggle_list)
    : toggle_list_(std::move(toggle_list)) {}

BrowserState::ToggleIntent::~ToggleIntent() = default;

std::string BrowserState::ToggleIntent::GetRequest() const {
  kidsmanagement::DefineChromeTestStateRequest request;
  for (const auto& toggle : toggle_list_) {
    if (toggle.type == FamilyLinkToggleType::kExtensionsToggle) {
      request.mutable_url_filtering_settings()->set_can_add_extensions(
          static_cast<bool>(toggle.state));
    }
    if (toggle.type == FamilyLinkToggleType::kPermissionsToggle) {
      request.mutable_url_filtering_settings()
          ->set_websites_can_request_permissions(
              static_cast<bool>(toggle.state));
    }
    if (toggle.type == FamilyLinkToggleType::kCookiesToggle) {
      request.mutable_url_filtering_settings()->set_can_block_cookies(
          static_cast<bool>(toggle.state));
    }
  }
  return request.SerializeAsString();
}

const FetcherConfig& BrowserState::ToggleIntent::GetConfig() const {
  return kDefineChromeTestStateConfig;
}

std::string BrowserState::ToggleIntent::ToString() const {
  std::vector<std::string> bits;
  bits.push_back("Define[");
  for (const auto& toggle : toggle_list_) {
    bits.push_back(GetToggleAbbrev(toggle.type) + " = ");
    bits.push_back((static_cast<bool>(toggle.state) ? "true" : "false") +
                   std::string(" "));
  }
  bits.push_back("]");
  return base::StrCat(bits);
}

bool BrowserState::ToggleIntent::Check(const FamilyMember& browser_user) const {
  bool result = true;
  for (const auto& toggle : toggle_list_) {
    bool toggle_has_expected_value =
        ToggleHasExpectedValue(browser_user, toggle);
    // Note: we do not exit the loop early on a false condition as we want
    // to print all the false conditions for debugging purposes.
    if (!toggle_has_expected_value) {
      LOG(WARNING) << "BrowserState::ToggleIntent[" +
                          GetToggleAbbrev(toggle.type) + "] = "
                   << (toggle_has_expected_value ? "true" : "false");
    }
    result = result && toggle_has_expected_value;
  }
  return result;
}

}  // namespace supervised_user
