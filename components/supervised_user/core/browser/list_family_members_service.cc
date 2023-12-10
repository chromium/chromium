// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/list_family_members_service.h"

#include <memory>
#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/common/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace supervised_user {

namespace {

// How often to refetch the family members.
constexpr base::TimeDelta kDefaultUpdateInterval = base::Days(1);
constexpr base::TimeDelta kOnErrorUpdateInterval = base::Hours(4);

base::TimeDelta NextUpdate(const ProtoFetcherStatus& status) {
  if (status.IsOk()) {
    return kDefaultUpdateInterval;
  }
  return kOnErrorUpdateInterval;
}

}  // namespace

ListFamilyMembersService::ListFamilyMembersService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {}

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
    const ProtoFetcherStatus& status,
    std::unique_ptr<kids_chrome_management::ListFamilyMembersResponse>
        response) {
  // Built-in mechanism for retrying will take care of internal retrying, but
  // the outer-loop of daily refetches is still implemented here. OnResponse
  // is only called when the fetcher encounters ultimate response: ok or
  // permanent error; retrying mechanism is abstracted away from this fetcher.
  CHECK(!status.IsTransientError());
  if (!status.IsOk()) {
    // This is unrecoverable persistent error from the fetcher (because
    // transient errors are indefinitely retried, see
    // RetryingFetcherImpl::OnResponse).
    CHECK(status.IsPersistentError());
    ScheduleNextUpdate(NextUpdate(status));
    return;
  }

  successful_fetch_consumers_.Notify(*response);
  ScheduleNextUpdate(NextUpdate(status));
}

void ListFamilyMembersService::ScheduleNextUpdate(base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay, this, &ListFamilyMembersService::Start);
}

}  // namespace supervised_user
