// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_MODEL_TYPE_CONTROLLER_DELEGATE_ANDROID_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_MODEL_TYPE_CONTROLLER_DELEGATE_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace password_manager {

// This class implements `syncer::ModelTypeControllerDelegate` but skips
// conntecting to sync engine. It will be used to disable password sync
// machinery and rely on GMS instead.
class PasswordModelTypeConrollerDelegateAndroid
    : public syncer::ModelTypeControllerDelegate {
 public:
  PasswordModelTypeConrollerDelegateAndroid();
  PasswordModelTypeConrollerDelegateAndroid(
      const PasswordModelTypeConrollerDelegateAndroid&) = delete;
  PasswordModelTypeConrollerDelegateAndroid(
      PasswordModelTypeConrollerDelegateAndroid&&) = delete;
  PasswordModelTypeConrollerDelegateAndroid& operator=(
      const PasswordModelTypeConrollerDelegateAndroid&) = delete;
  PasswordModelTypeConrollerDelegateAndroid& operator=(
      PasswordModelTypeConrollerDelegateAndroid&&) = delete;
  ~PasswordModelTypeConrollerDelegateAndroid() override;

  // syncer::ModelTypeControllerDelegate implementation.
  void OnSyncStarting(const syncer::DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(syncer::SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback)
      const override;
  void RecordMemoryUsageAndCountsHistograms() override;
  void ClearMetadataIfStopped() override;
  void ReportBridgeErrorForTest() override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetWeakPtrToBaseClass();

 private:
  base::WeakPtrFactory<PasswordModelTypeConrollerDelegateAndroid>
      weak_ptr_factory_{this};
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_MODEL_TYPE_CONTROLLER_DELEGATE_ANDROID_H_
