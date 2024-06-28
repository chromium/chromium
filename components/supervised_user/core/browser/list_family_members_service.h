// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_LIST_FAMILY_MEMBERS_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_LIST_FAMILY_MEMBERS_SERVICE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {
class IdentityManager;
}  // namespace signin

namespace supervised_user {

// List family members service. Manages the workflow of fetching the family
// member data from the RPC.
class ListFamilyMembersService : public KeyedService,
                                 public signin::IdentityManager::Observer {
 public:
  using SuccessfulFetchCallback =
      void(const kidsmanagement::ListMembersResponse&);

  ListFamilyMembersService() = delete;
  ListFamilyMembersService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService& user_prefs);
  ~ListFamilyMembersService() override;

  // Not copyable.
  ListFamilyMembersService(const ListFamilyMembersService&) = delete;
  ListFamilyMembersService& operator=(const ListFamilyMembersService&) = delete;

  void Init();

  // KeyedService:
  void Shutdown() override;

  // `callback` will receive every future update of family members until
  // unsubscribed by destroying the `base::CallbackListSubscription` handle.
  base::CallbackListSubscription SubscribeToSuccessfulFetches(
      base::RepeatingCallback<SuccessfulFetchCallback> callback);

 private:
  // signin::IdentityManager::Observer
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  void OnResponse(
      const ProtoFetcherStatus& status,
      std::unique_ptr<kidsmanagement::ListMembersResponse> response);
  void OnSuccess(const kidsmanagement::ListMembersResponse& response);
  void OnFailure(const ProtoFetcherStatus& status);
  void ScheduleNextUpdate(base::TimeDelta delay);

  // Utilities to start/stop fetching family account info.
  void StartRepeatedFetch();
  void StopFetch();

  // Updates prefs related to family account info.
  void SetFamilyMemberPrefs(
      const kidsmanagement::ListMembersResponse& list_members_response);

  // Dependencies.
  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ref<PrefService> user_prefs_;

  // Repeating consumers.
  base::RepeatingCallbackList<SuccessfulFetchCallback>
      successful_fetch_repeating_consumers_;

  // Observers.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};

  // Attributes.
  std::unique_ptr<ProtoFetcher<kidsmanagement::ListMembersResponse>> fetcher_;
  base::OneShotTimer timer_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_LIST_FAMILY_MEMBERS_SERVICE_H_
