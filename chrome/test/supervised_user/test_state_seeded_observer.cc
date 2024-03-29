// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/test_state_seeded_observer.h"

#include <memory>
#include <ostream>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/test/supervised_user/family_member.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
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

SupervisedUserService* GetSupervisedUserService(const FamilyMember& member) {
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(member.browser()->profile());
  CHECK(supervised_user_service) << "Incognito mode is not supported.";
  return supervised_user_service;
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

// Returns true iff the url doesn't contain `*` character or has different (or
// no) scheme than HTTP(s).
// Family link allows defining wildcarded urls, and  ::SupervisedUserUrlFilter
// has special behaviour for non-http Urls. Currently ChromeTestStateObserver
// doesn't support that urls.
bool IsPlainUrl(const GURL& gurl) {
  if (!gurl.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  if (base::Contains(gurl.spec(), "*")) {
    return false;
  }

  return true;
}

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

// Utility that groups writing filtering behavior request to the server,
// and verifying that the browser has received the settings.
struct FilterLevel {
  static void WriteToRequest(
      kidsmanagement::DefineChromeTestStateRequest& request,
      kidsmanagement::FilterLevel filter_level) {
    request.mutable_url_filtering_settings()->set_filter_level(filter_level);
  }

  static bool IsConfiguredForFamilyMember(
      const FamilyMember& member,
      kidsmanagement::FilterLevel filter_level) {
    SupervisedUserURLFilter* url_filter =
        GetSupervisedUserService(member)->GetURLFilter();
    CHECK(url_filter);

    PrefService* pref_service = member.browser()->profile()->GetPrefs();
    CHECK(pref_service);

    // See http://go/parentschromesupervision-dd
    switch (filter_level) {
      case kidsmanagement::ALLOW_BY_DEFAULT:
        return IsSafeSitesSetTo(*pref_service, false) &&
               IsFilteringBehaviourSetTo(*url_filter,
                                         FilteringBehavior::kAllow);
      case kidsmanagement::BLOCK_BY_DEFAULT:
        return IsSafeSitesSetTo(*pref_service, false) &&
               IsFilteringBehaviourSetTo(*url_filter,
                                         FilteringBehavior::kBlock);
      case kidsmanagement::SAFE_SITES:
        return IsSafeSitesSetTo(*pref_service, true) &&
               IsFilteringBehaviourSetTo(*url_filter,
                                         FilteringBehavior::kAllow);
      default:
        NOTREACHED_NORETURN();
    }
  }

 private:
  static bool IsFilteringBehaviourSetTo(const SupervisedUserURLFilter& filter,
                                        FilteringBehavior expected) {
    if (filter.GetDefaultFilteringBehavior() != expected) {
      LOG(WARNING) << "IsFilteringBehaviourSetTo: actual="
                   << filter.GetDefaultFilteringBehavior()
                   << " expected=" << expected;
    }
    return filter.GetDefaultFilteringBehavior() == expected;
  }

  static bool IsSafeSitesSetTo(const PrefService& pref_service,
                               bool is_enabled) {
    if (IsSafeSitesEnabled(pref_service) != is_enabled) {
      LOG(WARNING) << "IsSafeSitesSetTo: actual="
                   << IsSafeSitesEnabled(pref_service)
                   << " expected=" << is_enabled;
    }
    return IsSafeSitesEnabled(pref_service) == is_enabled;
  }
};

}  // namespace

void Delay(base::TimeDelta delay) {
  base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
  base::OneShotTimer timer;

  timer.Start(FROM_HERE, delay, &run_loop, &base::RunLoop::Quit);
  run_loop.Run();
}

void IssueResetOrDie(const FamilyMember& parent, const FamilyMember& child) {
  WaitForSuccessOrDie(
      CreateFetcher<kidsmanagement::ResetChromeTestStateResponse>(
          *parent.identity_manager(), parent.url_loader_factory(),
          kidsmanagement::ResetChromeTestStateRequest(),
          kResetChromeTestStateConfig, {child.GetAccountId().ToString()}));
}

void IssueDefineTestStateOrDie(const FamilyMember& parent,
                               const FamilyMember& child,
                               const std::vector<GURL>& allowed_urls,
                               const std::vector<GURL>& blocked_urls) {
  kidsmanagement::DefineChromeTestStateRequest request;
  for (auto&& url : allowed_urls) {
    AddWebsiteException(request, url, kidsmanagement::ALLOW);
  }
  for (auto&& url : blocked_urls) {
    AddWebsiteException(request, url, kidsmanagement::BLOCK);
  }
  FilterLevel::WriteToRequest(request, kidsmanagement::SAFE_SITES);

  WaitForSuccessOrDie(
      CreateFetcher<kidsmanagement::ResetChromeTestStateResponse>(
          *parent.identity_manager(), parent.url_loader_factory(), request,
          kDefineChromeTestStateConfig, {child.GetAccountId().ToString()}));
}

bool UrlFiltersAreConfigured(const FamilyMember& family_member,
                             const std::vector<GURL>& allowed_urls,
                             const std::vector<GURL>& blocked_urls) {
  SupervisedUserURLFilter* url_filter =
      GetSupervisedUserService(family_member)->GetURLFilter();
  CHECK(url_filter);

  if (!FilterLevel::IsConfiguredForFamilyMember(family_member,
                                                kidsmanagement::SAFE_SITES)) {
    return false;
  }

  for (const GURL& url : allowed_urls) {
    if (!IsUrlConfigured(*url_filter, url, FilteringBehavior::kAllow)) {
      LOG(WARNING) << url.spec()
                   << " is not configured yet (requested: kAllow).";
      return false;
    }
  }

  for (const GURL& url : blocked_urls) {
    if (!IsUrlConfigured(*url_filter, url, FilteringBehavior::kBlock)) {
      LOG(WARNING) << url.spec()
                   << " is not configured yet (requested: kBlock).";
      return false;
    }
  }
  return true;
}

bool UrlFiltersAreEmpty(const FamilyMember& family_member) {
  return GetSupervisedUserService(family_member)
      ->GetURLFilter()
      ->IsManualHostsEmpty();
}

ChromeTestStateSeedingResult
ChromeTestStateObserver::GetStateObserverInitialState() const {
  return ChromeTestStateSeedingResult::kWaitingForBrowserToPickUpChanges;
}

void ChromeTestStateObserver::OnURLFilterChanged() {
  if (!BrowserInIntendedState()) {
    LOG(WARNING) << name_ << " " << child().GetAccountId()
                 << " Not yet in the intended state";
    return;
  }

  OnStateObserverStateChanged(ChromeTestStateSeedingResult::kIntendedState);
}

ChromeTestStateObserver::ChromeTestStateObserver(std::string_view name,
                                                 const FamilyMember& child)
    : name_(name), child_(child) {
  GetSupervisedUserService(child)->AddObserver(this);
}

ChromeTestStateObserver::~ChromeTestStateObserver() {
  GetSupervisedUserService(*child_)->RemoveObserver(this);
}

DefineChromeTestStateObserver::DefineChromeTestStateObserver(
    const FamilyMember& child,
    const std::vector<GURL>& allowed_urls,
    const std::vector<GURL>& blocked_urls)
    : ChromeTestStateObserver("DefineChromeTestStateObserver", child),
      allowed_urls_(allowed_urls),
      blocked_urls_(blocked_urls) {
  for (auto&& gurl : allowed_urls) {
    CHECK(IsPlainUrl(gurl))
        << "Expected url with set protocol and no wildcards";
  }
  for (auto&& gurl : blocked_urls) {
    CHECK(IsPlainUrl(gurl))
        << "Expected url with set protocol and no wildcards";
  }
}
DefineChromeTestStateObserver::~DefineChromeTestStateObserver() = default;
bool DefineChromeTestStateObserver::BrowserInIntendedState() {
  return UrlFiltersAreConfigured(child(), allowed_urls_, blocked_urls_);
}

ResetChromeTestStateObserver::ResetChromeTestStateObserver(
    const FamilyMember& child)
    : ChromeTestStateObserver("ResetChromeTestStateObserver", child) {}
ResetChromeTestStateObserver::~ResetChromeTestStateObserver() = default;
bool ResetChromeTestStateObserver::BrowserInIntendedState() {
  return UrlFiltersAreEmpty(child());
}
}  // namespace supervised_user
