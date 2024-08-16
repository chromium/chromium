// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_SDK_DELEGATE_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_SDK_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#include "third_party/abseil-cpp/absl/status/status.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"

using base::android::ScopedJavaLocalRef;
#endif  // BUILDFLAG(IS_ANDROID)

namespace data_sharing {
class DataSharingNetworkLoader;

// Used by DataSharingService to provide access to SDK.
class DataSharingSDKDelegate {
 public:
  DataSharingSDKDelegate() = default;

  DataSharingSDKDelegate(const DataSharingSDKDelegate&) = delete;
  DataSharingSDKDelegate& operator=(const DataSharingSDKDelegate&) = delete;
  DataSharingSDKDelegate(DataSharingSDKDelegate&&) = delete;
  DataSharingSDKDelegate& operator=(DataSharingSDKDelegate&&) = delete;

  virtual ~DataSharingSDKDelegate() = default;

#if BUILDFLAG(IS_ANDROID)
  static std::unique_ptr<DataSharingSDKDelegate> CreateDelegate(
      ScopedJavaLocalRef<jobject> sdk_delegate);
#endif  // BUILDFLAG(IS_ANDROID)

  virtual void Initialize(
      DataSharingNetworkLoader* data_sharing_network_loader) = 0;

  virtual void CreateGroup(
      const data_sharing_pb::CreateGroupParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::CreateGroupResult,
                                    absl::Status>&)> callback) = 0;

  virtual void ReadGroups(
      const data_sharing_pb::ReadGroupsParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::ReadGroupsResult,
                                    absl::Status>&)> callback) = 0;

  virtual void AddMember(
      const data_sharing_pb::AddMemberParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) = 0;

  virtual void RemoveMember(
      const data_sharing_pb::RemoveMemberParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) = 0;

  virtual void DeleteGroup(
      const data_sharing_pb::DeleteGroupParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) = 0;

  virtual void LookupGaiaIdByEmail(
      const data_sharing_pb::LookupGaiaIdByEmailParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                                    absl::Status>&)> callback) = 0;

  virtual void Shutdown() {}
  virtual void AddAccessToken(
      const data_sharing_pb::AddAccessTokenParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::AddAccessTokenResult,
                                    absl::Status>&)> callback) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_SDK_DELEGATE_H_
