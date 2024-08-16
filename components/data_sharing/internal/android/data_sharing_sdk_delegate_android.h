// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_SDK_DELEGATE_ANDROID_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_SDK_DELEGATE_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"

using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace data_sharing {
class DataSharingNetworkLoaderAndroid;

// Helper class responsible for bridging the DataSharingSDKDelegate between
// C++ and Java.
class DataSharingSDKDelegateAndroid : public DataSharingSDKDelegate {
 public:
  using CreateGroupCallback = base::OnceCallback<void(
      const base::expected<data_sharing_pb::CreateGroupResult, absl::Status>&)>;

  using ReadGroupsCallback = base::OnceCallback<void(
      const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&)>;

  using LookupGaiaIdByEmailCallback = base::OnceCallback<void(
      const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                           absl::Status>&)>;

  using AddAccessTokenCallback = base::OnceCallback<void(
      const base::expected<data_sharing_pb::AddAccessTokenResult,
                           absl::Status>&)>;

  using GetStatusCallback = base::OnceCallback<void(const absl::Status&)>;

  explicit DataSharingSDKDelegateAndroid(const JavaRef<jobject>& sdk_delegate);
  ~DataSharingSDKDelegateAndroid() override;

  // Disallow copy/assign.
  DataSharingSDKDelegateAndroid(const DataSharingSDKDelegateAndroid&) = delete;
  DataSharingSDKDelegateAndroid& operator=(
      const DataSharingSDKDelegateAndroid&) = delete;

  DataSharingSDKDelegateAndroid(DataSharingSDKDelegateAndroid&&) = delete;
  DataSharingSDKDelegateAndroid& operator=(DataSharingSDKDelegateAndroid&&) =
      delete;

  // Gets the java side object.
  ScopedJavaLocalRef<jobject> GetJavaObject();

  // DataSharingSDKDelegate implementation.
  void Initialize(
      DataSharingNetworkLoader* data_sharing_network_loader) override;
  void CreateGroup(const data_sharing_pb::CreateGroupParams& params,
                   CreateGroupCallback callback) override;
  void ReadGroups(const data_sharing_pb::ReadGroupsParams& params,
                  ReadGroupsCallback callback) override;
  void AddMember(const data_sharing_pb::AddMemberParams& params,
                 GetStatusCallback callback) override;
  void RemoveMember(const data_sharing_pb::RemoveMemberParams& params,
                    GetStatusCallback callback) override;
  void DeleteGroup(const data_sharing_pb::DeleteGroupParams& params,
                   GetStatusCallback callback) override;
  void LookupGaiaIdByEmail(
      const data_sharing_pb::LookupGaiaIdByEmailParams& params,
      LookupGaiaIdByEmailCallback callback) override;
  void AddAccessToken(const data_sharing_pb::AddAccessTokenParams& params,
                      AddAccessTokenCallback callback) override;

 private:
  std::unique_ptr<DataSharingNetworkLoaderAndroid> network_loader_;

  // A reference to the Java counterpart of this class.  See
  // DataSharingSDKDelegateAndroid.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  base::WeakPtrFactory<DataSharingSDKDelegateAndroid> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_SDK_DELEGATE_ANDROID_H_
