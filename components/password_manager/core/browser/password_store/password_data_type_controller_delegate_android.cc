// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_data_type_controller_delegate_android.h"

#include "base/notreached.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/type_entities_count.h"

namespace password_manager {

PasswordDataTypeControllerDelegateAndroid::
    PasswordDataTypeControllerDelegateAndroid() = default;
PasswordDataTypeControllerDelegateAndroid::
    ~PasswordDataTypeControllerDelegateAndroid() = default;

void PasswordDataTypeControllerDelegateAndroid::OnSyncStarting(
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

void PasswordDataTypeControllerDelegateAndroid::OnSyncStopping(
    syncer::SyncStopMetadataFate metadata_fate) {}

void PasswordDataTypeControllerDelegateAndroid::HasUnsyncedData(
    base::OnceCallback<void(bool)> callback) {
  // No data is managed by PasswordDataTypeControllerDelegate - this datatype
  // doesn't use the built-in SyncEngine to communicate changes to/from the Sync
  // server; instead, Android-specific functionality is used for that. So there
  // can't be unsynced changes here.
  std::move(callback).Run(false);
}

void PasswordDataTypeControllerDelegateAndroid::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  // This is not implemented because it's not worth the hassle just to display
  // debug information in chrome://sync-internals.
  std::move(callback).Run(base::Value::List());
}

void PasswordDataTypeControllerDelegateAndroid::
    GetTypeEntitiesCountForDebugging(
        base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback)
        const {
  // This is not implemented because it's not worth the hassle just to display
  // debug information in chrome://sync-internals.
  std::move(callback).Run(syncer::TypeEntitiesCount(syncer::PASSWORDS));
}

void PasswordDataTypeControllerDelegateAndroid::
    RecordMemoryUsageAndCountsHistograms() {
  // This is not implemented because it's not worth the hassle. Password sync
  // module on Android doesn't hold any password. Instead passwords are
  // requested on demand from the GMS Core.
}

void PasswordDataTypeControllerDelegateAndroid::ClearMetadataIfStopped() {
  // No metadata is managed by PasswordDataTypeControllerDelegate.
}

void PasswordDataTypeControllerDelegateAndroid::ReportBridgeErrorForTest() {
  // Not supported for Android.
  NOTREACHED_IN_MIGRATION();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
PasswordDataTypeControllerDelegateAndroid::GetWeakPtrToBaseClass() {
  return weak_ptr_factory_.GetWeakPtr();
}
}  // namespace password_manager
