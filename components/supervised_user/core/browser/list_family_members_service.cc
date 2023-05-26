// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/list_family_members_service.h"

#include <memory>
#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace supervised_user {

namespace {

// How often to refetch the family members.
constexpr base::TimeDelta kUpdateInterval = base::Days(1);

// In case of an error while getting the family info, retry with exponential
// backoff.
const net::BackoffEntry::Policy kFamilyFetchBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    .num_errors_to_ignore = 0,

    // Initial delay for exponential backoff in ms.
    .initial_delay_ms = 2000,

    // Factor by which the waiting time will be multiplied.
    .multiply_factor = 2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    .jitter_factor = 0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    .maximum_backoff_ms = 1000 * 60 * 60 * 4,  // 4 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    .entry_lifetime_ms = -1,

    // Don't use initial delay unless the last request was an error.
    .always_use_initial_delay = false,
};

}  // namespace

ListFamilyMembersService::ListFamilyMembersService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      backoff_(&kFamilyFetchBackoffPolicy) {}

ListFamilyMembersService::~ListFamilyMembersService() = default;

base::CallbackListSubscription
ListFamilyMembersService::SubscribeToSuccessfulFetches(
    base::RepeatingCallback<SuccessfulFetchCallback> callback) {
  return successful_fetch_consumers_.Add(callback);
}

void ListFamilyMembersService::Start() {
  fetcher_ = FetchListFamilyMembers(
      *identity_manager_, url_loader_factory_,
      base::BindOnce(&ListFamilyMembersService::OnResponse,
                     base::Unretained(this)));
}

void ListFamilyMembersService::Cancel() {
  fetcher_.reset();
  timer_.Stop();
}

void ListFamilyMembersService::OnResponse(
    ProtoFetcherStatus status,
    std::unique_ptr<kids_chrome_management::ListFamilyMembersResponse>
        response) {
  if (!status.IsOk()) {
    OnFailure(status);
    return;
  }

  OnSuccess(*response);
  // Release response.
}

// The following methods handle the fetching of list family members.
void ListFamilyMembersService::OnSuccess(
    const kids_chrome_management::ListFamilyMembersResponse& response) {
  successful_fetch_consumers_.Notify(response);

  fetcher_.reset();
  backoff_.InformOfRequest(/*succeeded=*/true);
  ScheduleNextUpdate(kUpdateInterval);
}

void ListFamilyMembersService::OnFailure(ProtoFetcherStatus error) {
  DLOG(WARNING) << "ListFamilyMembers failed with status " << error.ToString();

  fetcher_.reset();
  backoff_.InformOfRequest(/*succeeded=*/false);
  ScheduleNextUpdate(backoff_.GetTimeUntilRelease());
}

void ListFamilyMembersService::ScheduleNextUpdate(base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay, this, &ListFamilyMembersService::Start);
}

}  // namespace supervised_user
