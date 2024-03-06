// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_SERVICE_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/data_sharing/public/group_data.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace data_sharing {
class DataSharingNetworkLoader;

// The core class for managing data sharing.
class DataSharingService : public KeyedService, public base::SupportsUserData {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    void OnGroupChanged(const GroupData& group_data);
    // User either created a new group or has been invited to the existing one.
    void OnGroupAdded(const GroupData& group_data);
    // Either group has been deleted or user has been removed from the group.
    void OnGroupRemoved(const std::string& group_id);
  };

  enum class PeopleGroupActionFailure { kTransientFailure, kPersistentFailure };

  enum class PeopleGroupActionOutcome {
    kSuccess,
    kTransientFailure,
    kPersistentFailure
  };

  using GroupDataOrFailureOutcome =
      base::expected<GroupData, PeopleGroupActionFailure>;
  using GroupsDataSetOrFailureOutcome =
      base::expected<std::set<GroupData>, PeopleGroupActionFailure>;

#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object of the type DataSharingService for the given
  // DataSharingService.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      DataSharingService* data_sharing_service);
#endif  // BUILDFLAG(IS_ANDROID)

  DataSharingService() = default;
  ~DataSharingService() override = default;

  // Disallow copy/assign.
  DataSharingService(const DataSharingService&) = delete;
  DataSharingService& operator=(const DataSharingService&) = delete;

  // Whether the service is an empty implementation. This is here because the
  // Chromium build disables RTTI, and we need to be able to verify that we are
  // using an empty service from the Chrome embedder.
  virtual bool IsEmptyService() = 0;

  // Returns the network loader for fetching data.
  virtual DataSharingNetworkLoader* GetDataSharingNetworkLoader() = 0;

  // People Group API.
  // Refreshes data if necessary. On success passes to the `callback` a set of
  // all groups known to the client (ordered by id).
  virtual void ReadAllGroups(
      base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)>
          callback) = 0;

  // Refreshes data if necessary and passes the GroupData to `callback`.
  virtual void ReadGroup(
      const std::string& group_id,
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) = 0;

  // Attempts to create a new group. Returns a created group on success.
  virtual void CreateGroup(
      const std::string& group_name,
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) = 0;

  // Attempts to delete a group.
  virtual void DeleteGroup(
      const std::string& group_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) = 0;

  // Attempts to invite a new user to the group.
  virtual void InviteMember(
      const std::string& group_id,
      const std::string& invitee_gaia_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) = 0;

  // Attempts to remove a user from the group.
  virtual void RemoveMember(
      const std::string& group_id,
      const std::string& member_gaia_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_SERVICE_H_
