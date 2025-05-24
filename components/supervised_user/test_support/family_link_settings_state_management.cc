// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/test_support/family_link_settings_state_management.h"

#include <memory>
#include <optional>
#include <ostream>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/version_info/channel.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/browser/proto_fetcher_status.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/testutil_proto_fetcher.h"
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
      NOTREACHED();
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

bool AreSafeSitesConfigured(const FamilyLinkSettingsState::Services& services) {
  return IsSafeSitesEnabled(services.pref_service.get());
}

bool IsUrlConfigured(SupervisedUserURLFilter& url_filter,
                     const GURL& url,
                     FilteringBehavior expected_filtering_behavior) {
  SupervisedUserURLFilter::Result result = url_filter.GetFilteringBehavior(url);

  if (!result.IsFromManualList()) {
    // The change that arrived doesn't have the manual mode for requested url
    // - wait for the next one.
    LOG(WARNING) << "IsUrlConfigured: no manual mode for " << url.spec();
    return false;
  }

  if (result.behavior != expected_filtering_behavior) {
    LOG(WARNING) << "IsUrlConfigured: filtering behavior mismatch, actual="
                 << result.behavior
                 << " expected=" << expected_filtering_behavior;
    return false;
  }
  return true;
}

bool UrlFiltersAreConfigured(const FamilyLinkSettingsState::Services& services,
                             const std::optional<GURL>& allowed_url,
                             const std::optional<GURL>& blocked_url) {
  SupervisedUserURLFilter* url_filter =
      services.supervised_user_service->GetURLFilter();
  CHECK(url_filter);

  if (!AreSafeSitesConfigured(services)) {
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

bool UrlFiltersAreEmpty(const FamilyLinkSettingsState::Services& services) {
  return services.supervised_user_service->GetURLFilter()
             ->GetFilteringStatistics()
             .GetManagedSiteList() ==
         SupervisedUserURLFilter::ManagedSiteList::kEmpty;
}

bool ToggleHasExpectedValue(const FamilyLinkSettingsState::Services& services,
                            FamilyLinkToggleConfiguration toggle) {
  content_settings::ProviderType provider_type;

  if (toggle.type == FamilyLinkToggleType::kCookiesToggle) {
    bool can_block_cookies = static_cast<bool>(toggle.state);
    services.host_content_settings_map->GetDefaultContentSetting(
        ContentSettingsType::COOKIES, &provider_type);
    // The supervised user can block the cookies if the corresponding content
    // provider is not supervised.
    return can_block_cookies ==
           (provider_type !=
            content_settings::ProviderType::kSupervisedProvider);
  }
  if (toggle.type == FamilyLinkToggleType::kPermissionsToggle) {
    bool permission_pref_has_expected_value =
        services.pref_service->GetBoolean(
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
        (services.host_content_settings_map->GetDefaultContentSetting(
             ContentSettingsType::GEOLOCATION, &provider_type) ==
         ContentSetting::CONTENT_SETTING_BLOCK);

    return permission_pref_has_expected_value && is_geolocation_configured;
  }
  CHECK(toggle.type == FamilyLinkToggleType::kExtensionsToggle);
  return services.pref_service->GetBoolean(
             prefs::kSkipParentApprovalToInstallExtensions) ==
         static_cast<bool>(toggle.state);
}
}  // namespace

FamilyLinkSettingsState::~FamilyLinkSettingsState() = default;
FamilyLinkSettingsState::FamilyLinkSettingsState(
    std::unique_ptr<Intent> intent) {
  intent_ = std::move(intent);
}

FamilyLinkSettingsState FamilyLinkSettingsState::Reset() {
  return FamilyLinkSettingsState(std::make_unique<ResetIntent>());
}
FamilyLinkSettingsState FamilyLinkSettingsState::EnableSafeSites() {
  return FamilyLinkSettingsState(
      std::make_unique<DefineManualSiteListIntent>());
}
FamilyLinkSettingsState FamilyLinkSettingsState::AllowSite(const GURL& gurl) {
  return FamilyLinkSettingsState(std::make_unique<DefineManualSiteListIntent>(
      DefineManualSiteListIntent::AllowUrl(gurl)));
}
FamilyLinkSettingsState FamilyLinkSettingsState::BlockSite(const GURL& gurl) {
  return FamilyLinkSettingsState(std::make_unique<DefineManualSiteListIntent>(
      DefineManualSiteListIntent::BlockUrl(gurl)));
}
FamilyLinkSettingsState FamilyLinkSettingsState::AdvancedSettingsToggles(
    std::list<FamilyLinkToggleConfiguration> toggle_list) {
  return FamilyLinkSettingsState(
      std::make_unique<ToggleIntent>(std::move(toggle_list)));
}

FamilyLinkSettingsState FamilyLinkSettingsState::SetAdvancedSettingsDefault() {
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

#if !BUILDFLAG(IS_IOS)
void FamilyLinkSettingsState::Seed(
    signin::IdentityManager& caller_identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> caller_url_loader_factory,
    std::string_view subject_account_id) const {
  // Start fetching and wait for the response.
  base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
  ProtoFetcher<std::string> fetcher(
      caller_identity_manager, caller_url_loader_factory,
      {.request_body = intent_->GetRequest()},
      base::BindLambdaForTesting([&](const ProtoFetcherStatus& status,
                                     std::unique_ptr<std::string> response) {
        CHECK(status.IsOk()) << "WaitForRequestToComplete failed with status: "
                             << status.ToString();
        run_loop.Quit();
      }),

      intent_->GetConfig(), {std::string(subject_account_id)},
      version_info::Channel::UNKNOWN);
  run_loop.Run();
}
#endif

#if BUILDFLAG(IS_IOS)
void FamilyLinkSettingsState::StartSeeding(
    signin::IdentityManager& caller_identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> caller_url_loader_factory,
    std::string_view subject_account_id) {
  // Do not override the current fetch.
  CHECK(fetcher_ == nullptr);

  // Start fetching.
  fetcher_ = CreateFetcher<std::string>(
      caller_identity_manager, caller_url_loader_factory,
      {.request_body = intent_->GetRequest()},
      base::BindOnce([](const ProtoFetcherStatus& status,
                        std::unique_ptr<std::string> response) {
        CHECK(status.IsOk()) << "WaitForRequestToComplete failed with status: "
                             << status.ToString();
      }),
      intent_->GetConfig(), {std::string(subject_account_id)},
      version_info::Channel::UNKNOWN);
}
#endif  // BUILDFLAG(IS_IOS)

bool FamilyLinkSettingsState::Check(const Services& services) const {
  return intent_->Check(services);
}

std::string FamilyLinkSettingsState::ToString() const {
  return intent_->ToString();
}

FamilyLinkSettingsState::Intent::~Intent() = default;

FamilyLinkSettingsState::ResetIntent::~ResetIntent() = default;
std::string FamilyLinkSettingsState::ResetIntent::GetRequest() const {
  return kidsmanagement::ResetChromeTestStateRequest().SerializeAsString();
}
const FetcherConfig& FamilyLinkSettingsState::ResetIntent::GetConfig() const {
  return kResetChromeTestStateConfig;
}
std::string FamilyLinkSettingsState::ResetIntent::ToString() const {
  return "Reset";
}
bool FamilyLinkSettingsState::ResetIntent::Check(
    const FamilyLinkSettingsState::Services& services) const {
  bool result = UrlFiltersAreEmpty(services);
  LOG(WARNING) << "FamilyLinkSettingsState::ResetIntent = "
               << base::ToString(result);
  return result;
}

FamilyLinkSettingsState::DefineManualSiteListIntent::
    DefineManualSiteListIntent() = default;
FamilyLinkSettingsState::DefineManualSiteListIntent::DefineManualSiteListIntent(
    AllowUrl url)
    : allowed_url_(url) {}
FamilyLinkSettingsState::DefineManualSiteListIntent::DefineManualSiteListIntent(
    BlockUrl url)
    : blocked_url_(url) {}
FamilyLinkSettingsState::DefineManualSiteListIntent::
    ~DefineManualSiteListIntent() = default;

std::string FamilyLinkSettingsState::DefineManualSiteListIntent::GetRequest()
    const {
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
FamilyLinkSettingsState::DefineManualSiteListIntent::GetConfig() const {
  return kDefineChromeTestStateConfig;
}
std::string FamilyLinkSettingsState::DefineManualSiteListIntent::ToString()
    const {
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
bool FamilyLinkSettingsState::DefineManualSiteListIntent::Check(
    const FamilyLinkSettingsState::Services& services) const {
  bool result = UrlFiltersAreConfigured(services, allowed_url_, blocked_url_);
  LOG(WARNING) << "FamilyLinkSettingsState::DefineManualSiteListIntent = "
               << base::ToString(result);
  return result;
}

FamilyLinkSettingsState::ToggleIntent::ToggleIntent(
    std::list<FamilyLinkToggleConfiguration> toggle_list)
    : toggle_list_(std::move(toggle_list)) {}

FamilyLinkSettingsState::ToggleIntent::~ToggleIntent() = default;

std::string FamilyLinkSettingsState::ToggleIntent::GetRequest() const {
  kidsmanagement::DefineChromeTestStateRequest request;
  for (const auto& toggle : toggle_list_) {
    if (toggle.type == FamilyLinkToggleType::kExtensionsToggle) {
      request.set_can_add_extensions(static_cast<bool>(toggle.state));
    }
    if (toggle.type == FamilyLinkToggleType::kPermissionsToggle) {
      request.set_websites_can_request_permissions(
          static_cast<bool>(toggle.state));
    }
    if (toggle.type == FamilyLinkToggleType::kCookiesToggle) {
      request.set_can_block_cookies(static_cast<bool>(toggle.state));
    }
  }
  return request.SerializeAsString();
}

const FetcherConfig& FamilyLinkSettingsState::ToggleIntent::GetConfig() const {
  return kDefineChromeTestStateConfig;
}

std::string FamilyLinkSettingsState::ToggleIntent::ToString() const {
  std::vector<std::string> bits;
  bits.push_back("Define[");
  for (const auto& toggle : toggle_list_) {
    bits.push_back(GetToggleAbbrev(toggle.type) + " = ");
    bits.push_back(base::ToString(static_cast<bool>(toggle.state)) +
                   std::string(" "));
  }
  bits.push_back("]");
  return base::StrCat(bits);
}

bool FamilyLinkSettingsState::ToggleIntent::Check(
    const FamilyLinkSettingsState::Services& services) const {
  bool result = true;
  for (const auto& toggle : toggle_list_) {
    bool toggle_has_expected_value = ToggleHasExpectedValue(services, toggle);
    // Note: we do not exit the loop early on a false condition as we want
    // to print all the false conditions for debugging purposes.
    if (!toggle_has_expected_value) {
      LOG(WARNING) << "FamilyLinkSettingsState::ToggleIntent[" +
                          GetToggleAbbrev(toggle.type) + "] = "
                   << base::ToString(toggle_has_expected_value);
    }
    result = result && toggle_has_expected_value;
  }
  return result;
}

FamilyLinkSettingsState::Services::Services(
    const SupervisedUserService& supervised_user_service,
    const PrefService& pref_service,
    const HostContentSettingsMap& host_content_settings_map)
    : supervised_user_service(supervised_user_service),
      pref_service(pref_service),
      host_content_settings_map(host_content_settings_map) {}

}  // namespace supervised_user
