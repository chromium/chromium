// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_DATA_TYPE_CONTROLLER_DELEGATE_ANDROID_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_DATA_TYPE_CONTROLLER_DELEGATE_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "components/sync/model/data_type_controller_delegate.h"

namespace password_manager {

// This class implements `syncer::DataTypeControllerDelegate` but skips
// conntecting to sync engine. It will be used to disable password sync
// machinery and rely on GMS instead.
class PasswordDataTypeControllerDelegateAndroid
    : public syncer::DataTypeControllerDelegate {
 public:
  PasswordDataTypeControllerDelegateAndroid();
  PasswordDataTypeControllerDelegateAndroid(
      const PasswordDataTypeControllerDelegateAndroid&) = delete;
  PasswordDataTypeControllerDelegateAndroid(
      PasswordDataTypeControllerDelegateAndroid&&) = delete;
  PasswordDataTypeControllerDelegateAndroid& operator=(
      const PasswordDataTypeControllerDelegateAndroid&) = delete;
  PasswordDataTypeControllerDelegateAndroid& operator=(
      PasswordDataTypeControllerDelegateAndroid&&) = delete;
  ~PasswordDataTypeControllerDelegateAndroid() override;

  // syncer::DataTypeControllerDelegate implementation.
  void OnSyncStarting(const syncer::DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(syncer::SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void HasUnsyncedData(base::OnceCallback<void(bool)> callback) override;
  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback)
      const override;
  void RecordMemoryUsageAndCountsHistograms() override;
  void ClearMetadataIfStopped() override;
  void ReportBridgeErrorForTest() override;

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetWeakPtrToBaseClass();

 private:
  base::WeakPtrFactory<PasswordDataTypeControllerDelegateAndroid>
      weak_ptr_factory_{this};
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_DATA_TYPE_CONTROLLER_DELEGATE_ANDROID_H_
