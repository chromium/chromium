// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_TEST_STATE_SEEDED_OBSERVER_H_
#define CHROME_TEST_SUPERVISED_USER_TEST_STATE_SEEDED_OBSERVER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "chrome/test/supervised_user/family_member.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "ui/base/interaction/state_observer.h"

namespace supervised_user {

// List of possible results of data seeding that can be expected in test
// sequences.
enum class ChromeTestStateSeedingResult {
  kIntendedState = 0,
  kPendingRpcResponse = 1,
  kWaitingForBrowserToPickUpChanges = 2,
};

// Base class for test state observers that can alter the chrome browse state by
// issuing an RPC to Google backends that are later picked up by browser and
// used to configure the supervised user behaviors.
//
// ChromeTestStateObserver can:
// * tell if the browser is already at intended state,
// * issue RPC that changes that state if needed and wait until the state was
// achieved.
class ChromeTestStateObserver
    : public ui::test::StateObserver<ChromeTestStateSeedingResult>,
      public SupervisedUserServiceObserver {
 public:
  ChromeTestStateObserver() = delete;
  ~ChromeTestStateObserver() override;

  // The expected state is verified on `child` browser; the RPC is issued by
  // `parent`.
  ChromeTestStateObserver(const FamilyMember& parent,
                          const FamilyMember& child);
  ChromeTestStateObserver(const ChromeTestStateObserver& other) = delete;
  ChromeTestStateObserver& operator=(const ChromeTestStateObserver& other) =
      delete;

  // Indicates whether the browser is in state that reflects requested state.
  virtual bool InIntendedState() const = 0;

  // Return initial state of the child browser from the prefs: kPending in case
  // when the seeding is required, or kSuccessful when the state is ready.
  ChromeTestStateSeedingResult GetStateObserverInitialState() const override;

  // SupervisedUserServiceObserver
  void OnURLFilterChanged() override;

 protected:
  virtual void StartRpc() const = 0;

  // Asserts that the RPC was successful, but doesn't yet transition to
  // ChromeTestStateSeedingResult::kIntendedState, instead sets the current
  // state to ChromeTestStateSeedingResult::kWaitingForBrowserToPickUpChanges as
  // now the browser must receive the changes.
  void HandleRpcStatus(const supervised_user::ProtoFetcherStatus& status);

  const FamilyMember& child() const { return *child_; }
  const FamilyMember& parent() const { return *parent_; }

 protected:
  template <typename Response>
  ProtoFetcher<Response>::Callback CreateCallback() {
    return base::BindLambdaForTesting(
        [this](const ProtoFetcherStatus& status,
               std::unique_ptr<Response> response) -> void {
          this->HandleRpcStatus(status);
        });
  }

 private:
  // Requests are executed on behalf of `parent_`.
  raw_ref<const FamilyMember> parent_;
  // Requests effects affect `child_` user.
  raw_ref<const FamilyMember> child_;
};

// Sets the browser state so that requested urls are either allowed or blocked.
// Filter level is intended to be `SAFE_SITES`.
class DefineChromeTestStateObserver : public ChromeTestStateObserver {
 public:
  ~DefineChromeTestStateObserver() override;

  // The expected state is verified on `child` browser; the RPC is issued by
  // `parent`.
  DefineChromeTestStateObserver(const FamilyMember& parent,
                                const FamilyMember& child,
                                const std::vector<GURL>& allowed_urls,
                                const std::vector<GURL>& blocked_urls);

  bool InIntendedState() const override;

 protected:
  void StartRpc() const override;

 private:
  static constexpr kids_chrome_management::FilterLevel kFilterLevel{
      kids_chrome_management::SAFE_SITES};

  kids_chrome_management::DefineChromeTestStateRequest CreateRequest() const;

  // Returns true iff the `filter` correctly reflects the intended state of both
  // `allowed_urls_` and `blocked_urls_`.
  bool AllUrlsAreConfigured(SupervisedUserURLFilter& filter) const;

  const std::vector<GURL> allowed_urls_;
  const std::vector<GURL> blocked_urls_;

  // Is mutable to satisfy ::StartRpc() const (called from
  // ::GetStateObserverInitialState const).
  mutable std::unique_ptr<
      ProtoFetcher<kids_chrome_management::DefineChromeTestStateResponse>>
      fetcher_;

  mutable ProtoFetcher<kids_chrome_management::
                           DefineChromeTestStateResponse>::Callback callback_ =
      CreateCallback<kids_chrome_management::DefineChromeTestStateResponse>();
};

// Sets the browser state so that no urls are either allowed or blocked.
class ResetChromeTestStateObserver : public ChromeTestStateObserver {
 public:
  ~ResetChromeTestStateObserver() override;

  // The expected state is verified on `child` browser; the RPC is issued by
  // `parent`.
  ResetChromeTestStateObserver(const FamilyMember& parent,
                               const FamilyMember& child);

  bool InIntendedState() const override;

 protected:
  void StartRpc() const override;

 private:
  // Is mutable to satisfy ::StartRpc() const (called from
  // ::GetStateObserverInitialState const).
  mutable std::unique_ptr<
      ProtoFetcher<kids_chrome_management::ResetChromeTestStateResponse>>
      fetcher_;

  mutable ProtoFetcher<kids_chrome_management::
                           ResetChromeTestStateResponse>::Callback callback_ =
      CreateCallback<kids_chrome_management::ResetChromeTestStateResponse>();
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_TEST_STATE_SEEDED_OBSERVER_H_
