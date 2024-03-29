// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_TEST_STATE_SEEDED_OBSERVER_H_
#define CHROME_TEST_SUPERVISED_USER_TEST_STATE_SEEDED_OBSERVER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/test/supervised_user/family_member.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "ui/base/interaction/state_observer.h"

namespace supervised_user {

// List of possible results of data seeding that can be expected in test
// sequences.
enum class ChromeTestStateSeedingResult {
  kIntendedState,
  kWaitingForBrowserToPickUpChanges,
};

// Checks if the `family_member`'s browser filters `allowed_urls` and
// `blocked_urls` by examining
// SupervisedUserURLFilter::GetManualFilteringBehaviorForURL status for each
// url.
bool UrlFiltersAreConfigured(const FamilyMember& family_member,
                             const std::vector<GURL>& allowed_urls,
                             const std::vector<GURL>& blocked_urls);
// Checks if the `family_member`'s browser has empty filters.
bool UrlFiltersAreEmpty(const FamilyMember& family_member);

void Delay(base::TimeDelta delay);

// Expects successful backend response (HTTP 200) for the fetch, crashes
// otherwise.
template <class Response>
void WaitForSuccessOrDie(std::unique_ptr<ProtoFetcher<Response>> fetcher) {
  base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
  fetcher->Start(base::BindLambdaForTesting(
      [&](const ProtoFetcherStatus& status,
          std::unique_ptr<Response> response) -> void {
        CHECK(status.IsOk())
            << "Test seeding failed with status: " << status.ToString();
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Issues ResetChromeTestState RPC and expects that it will succeed.
void IssueResetOrDie(const FamilyMember& parent, const FamilyMember& child);

// Issues DefineChromeTestState RPC and expects that it will succeed.
void IssueDefineTestStateOrDie(const FamilyMember& parent,
                               const FamilyMember& child,
                               const std::vector<GURL>& allowed_urls,
                               const std::vector<GURL>& blocked_urls);

// Base class for test state observers. They are waiting until the browser is in
// the intended state. ChromeTestStateObserver assumes that the browser is not
// in the intended state.
class ChromeTestStateObserver
    : public ui::test::StateObserver<ChromeTestStateSeedingResult>,
      public SupervisedUserServiceObserver {
 public:
  // The expected state is verified on `child` browser; the RPC is issued by
  // `parent`.
  ChromeTestStateObserver(std::string_view name, const FamilyMember& child);
  ChromeTestStateObserver(const ChromeTestStateObserver& other) = delete;
  ChromeTestStateObserver& operator=(const ChromeTestStateObserver& other) =
      delete;
  ~ChromeTestStateObserver() override;

  // This observer should be used when state change is expected, and starts in
  // ChromeTestStateSeedingResult::kWaitingForBrowserToPickUpChanges state.
  ChromeTestStateSeedingResult GetStateObserverInitialState() const override;

  // SupervisedUserServiceObserver
  void OnURLFilterChanged() override;

 protected:
  virtual bool BrowserInIntendedState() = 0;

  // Asserts that the RPC was successful, but doesn't yet transition to
  // ChromeTestStateSeedingResult::kIntendedState, instead sets the current
  // state to ChromeTestStateSeedingResult::kWaitingForBrowserToPickUpChanges as
  // now the browser must receive the changes.
  void HandleRpcStatus(const supervised_user::ProtoFetcherStatus& status);

  const FamilyMember& child() const { return *child_; }

 private:
  // Unique name of this fetcher, for logging.
  std::string name_;
  // Requests effects affect `child_` user.
  raw_ref<const FamilyMember> child_;
};

// Sets the browser state so that requested urls are either allowed or blocked.
// Filter level is intended to be `SAFE_SITES`.
class DefineChromeTestStateObserver : public ChromeTestStateObserver {
 public:
  // The expected state is verified on `child` browser; the RPC is issued by
  // `parent`.
  DefineChromeTestStateObserver(const FamilyMember& child,
                                const std::vector<GURL>& allowed_urls,
                                const std::vector<GURL>& blocked_urls);
  ~DefineChromeTestStateObserver() override;

 protected:
  bool BrowserInIntendedState() override;

 private:
  static constexpr kidsmanagement::FilterLevel kFilterLevel{
      kidsmanagement::SAFE_SITES};
  const std::vector<GURL> allowed_urls_;
  const std::vector<GURL> blocked_urls_;
};

// Sets the browser state so that no urls are either allowed or blocked.
class ResetChromeTestStateObserver : public ChromeTestStateObserver {
 public:
  // The expected state is verified on `child` browser; the RPC is issued by
  // `parent`.
  explicit ResetChromeTestStateObserver(const FamilyMember& child);
  ~ResetChromeTestStateObserver() override;

 protected:
  bool BrowserInIntendedState() override;
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_TEST_STATE_SEEDED_OBSERVER_H_
