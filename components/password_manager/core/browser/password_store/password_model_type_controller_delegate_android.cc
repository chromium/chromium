// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_model_type_controller_delegate_android.h"

#include "base/notreached.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/type_entities_count.h"

namespace password_manager {

PasswordModelTypeConrollerDelegateAndroid::
    PasswordModelTypeConrollerDelegateAndroid() = default;
PasswordModelTypeConrollerDelegateAndroid::
    ~PasswordModelTypeConrollerDelegateAndroid() = default;

void PasswordModelTypeConrollerDelegateAndroid::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request,
    StartCallback callback) {
  // Set `skip_engine_connection` to true to indicate that, actually, this sync
  // datatype doesn't depend on the built-in SyncEngine to communicate changes
  // to/from the Sync server. Instead, Android specific functionality is
  // leveraged to achieve similar behavior.
  auto activation_response =
      std::make_unique<syncer::DataTypeActivationResponse>();
  activation_response->skip_engine_connection = true;
  std::move(callback).Run(std::move(activation_response));
}

void PasswordModelTypeConrollerDelegateAndroid::OnSyncStopping(
    syncer::SyncStopMetadataFate metadata_fate) {}

void PasswordModelTypeConrollerDelegateAndroid::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  // This is not implemented because it's not worth the hassle just to display
  // debug information in chrome://sync-internals.
  std::move(callback).Run(syncer::PASSWORDS, base::Value::List());
}

void PasswordModelTypeConrollerDelegateAndroid::
    GetTypeEntitiesCountForDebugging(
        base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback)
        const {
  // This is not implemented because it's not worth the hassle just to display
  // debug information in chrome://sync-internals.
  std::move(callback).Run(syncer::TypeEntitiesCount(syncer::PASSWORDS));
}

void PasswordModelTypeConrollerDelegateAndroid::
    RecordMemoryUsageAndCountsHistograms() {
  // This is not implemented because it's not worth the hassle. Password sync
  // module on Android doesn't hold any password. Instead passwords are
  // requested on demand from the GMS Core.
}

void PasswordModelTypeConrollerDelegateAndroid::ClearMetadataIfStopped() {
  // No metadata is managed by PasswordModelTypeControllerDelegate.
}

void PasswordModelTypeConrollerDelegateAndroid::ReportBridgeErrorForTest() {
  // Not supported for Android.
  NOTREACHED();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
PasswordModelTypeConrollerDelegateAndroid::GetWeakPtrToBaseClass() {
  return weak_ptr_factory_.GetWeakPtr();
}
}  // namespace password_manager
