// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SDK_DELEGATE_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SDK_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/data_sharing/internal/protocol/data_sharing_sdk.pb.h"

namespace data_sharing {

// Used by DataSharingService to provide access to SDK.
class DataSharingSDKDelegate {
 public:
  enum class Failure {
    TRANSIENT_FAILURE,
    PERSISTENT_FAILURE,
  };

  DataSharingSDKDelegate() = default;

  DataSharingSDKDelegate(const DataSharingSDKDelegate&) = delete;
  DataSharingSDKDelegate& operator=(const DataSharingSDKDelegate&) = delete;
  DataSharingSDKDelegate(DataSharingSDKDelegate&&) = delete;
  DataSharingSDKDelegate& operator=(DataSharingSDKDelegate&&) = delete;

  virtual ~DataSharingSDKDelegate() = default;

  virtual void CreateGroup(
      const std::string& display_name,
      base::OnceCallback<
          void(base::expected<data_sharing_pb::CreateGroupResponse, Failure>)>
          callback) = 0;

  virtual void ReadGroups(
      const std::vector<std::string>& group_ids,
      base::OnceCallback<
          void(base::expected<data_sharing_pb::ReadGroupsResponse, Failure>)>
          callback) = 0;

  virtual void ReadAllGroups(
      base::OnceCallback<
          void(base::expected<data_sharing_pb::ReadAllGroupsResponse, Failure>)>
          callback) = 0;

  virtual void UpdateGroup(
      const std::string& group_id,
      const std::vector<data_sharing_pb::GroupUpdate>& updates,
      base::OnceCallback<
          void(base::expected<data_sharing_pb::UpdateGroupResponse, Failure>)>
          callback) = 0;

  virtual void DeleteGroups(
      const std::vector<std::string>& group_ids,
      base::OnceCallback<
          void(base::expected<data_sharing_pb::DeleteGroupsResponse, Failure>)>
          callback) = 0;

  virtual void LookupGaiaIdByEmail(
      const std::string& email,
      base::OnceCallback<void(base::expected<std::string /*email*/, Failure>)>
          callback) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SDK_DELEGATE_H_
