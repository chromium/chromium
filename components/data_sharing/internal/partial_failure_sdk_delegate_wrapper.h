// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_PARTIAL_FAILURE_SDK_DELEGATE_WRAPPER_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_PARTIAL_FAILURE_SDK_DELEGATE_WRAPPER_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "components/data_sharing/public/group_data.h"

namespace data_sharing {

// Wraps another SDK delegate and adds support for ReadGroups() partial
// failures: instead of issuing a single SDK::ReadGroups() call with multiple
// group_ids, it issues multiple SDK::ReadGroups() calls, each one requesting a
// single group_id. The rest of the methods are simply forwarded to the wrapped
// SDK delegate.
// WARNING: this class only supports single ongoing ReadGroups() call at a time.
// TODO(crbug.com/377914193): Remove this class once SDK supports partial
// failures natively.
class PartialFailureSDKDelegateWrapper : public DataSharingSDKDelegate {
 public:
  explicit PartialFailureSDKDelegateWrapper(
      DataSharingSDKDelegate* sdk_delegate);

  PartialFailureSDKDelegateWrapper(const PartialFailureSDKDelegateWrapper&) =
      delete;
  PartialFailureSDKDelegateWrapper& operator=(
      const PartialFailureSDKDelegateWrapper&) = delete;
  PartialFailureSDKDelegateWrapper(PartialFailureSDKDelegateWrapper&&) = delete;
  PartialFailureSDKDelegateWrapper& operator=(
      PartialFailureSDKDelegateWrapper&&) = delete;

  ~PartialFailureSDKDelegateWrapper() override;

  void Initialize(
      DataSharingNetworkLoader* data_sharing_network_loader) override;

  void CreateGroup(const data_sharing_pb::CreateGroupParams& params,
                   base::OnceCallback<void(
                       const base::expected<data_sharing_pb::CreateGroupResult,
                                            absl::Status>&)> callback) override;

  void ReadGroups(const data_sharing_pb::ReadGroupsParams& params,
                  base::OnceCallback<void(
                      const base::expected<data_sharing_pb::ReadGroupsResult,
                                           absl::Status>&)> callback) override;

  void AddMember(
      const data_sharing_pb::AddMemberParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override;

  void RemoveMember(
      const data_sharing_pb::RemoveMemberParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override;

  void LeaveGroup(
      const data_sharing_pb::LeaveGroupParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override;

  void DeleteGroup(
      const data_sharing_pb::DeleteGroupParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override;

  void LookupGaiaIdByEmail(
      const data_sharing_pb::LookupGaiaIdByEmailParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                                    absl::Status>&)> callback) override;

  void Shutdown() override;
  void AddAccessToken(
      const data_sharing_pb::AddAccessTokenParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::AddAccessTokenResult,
                                    absl::Status>&)> callback) override;

 private:
  void OnSingleReadGroupCompleted(
      const GroupId& group_id,
      const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&
          result);
  void OnAllOngoingReadGroupsCompleted();

  raw_ptr<DataSharingSDKDelegate> sdk_delegate_;

  size_t ongoing_read_groups_groups_count_ = 0;
  base::OnceCallback<void(
      const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&)>
      ongoing_read_groups_callback_;
  std::map<GroupId,
           base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>>
      finished_read_group_results_;

  base::WeakPtrFactory<PartialFailureSDKDelegateWrapper> weak_ptr_factory_{
      this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_PARTIAL_FAILURE_SDK_DELEGATE_WRAPPER_H_
