// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id.h"

#include "base/functional/bind.h"
#include "components/gcm_driver/gcm_driver.h"

namespace instance_id {

// A common use case for InstanceID tokens is to authorize and route push
// messages sent via Google Cloud Messaging (replacing the earlier registration
// IDs). To get such a GCM-enabled token, pass this scope to getToken.
// Must match Java GoogleCloudMessaging.INSTANCE_ID_SCOPE.
const char kGCMScope[] = "GCM";

InstanceID::InstanceID(const std::string& app_id, gcm::GCMDriver* gcm_driver)
    : gcm_driver_(gcm_driver), app_id_(app_id) {}

InstanceID::~InstanceID() = default;

void InstanceID::GetEncryptionInfo(const std::string& authorized_entity,
                                   GetEncryptionInfoCallback callback) {
  gcm_driver_->GetEncryptionProviderInternal()->GetEncryptionInfo(
      app_id_, authorized_entity, std::move(callback));
}

void InstanceID::DeleteToken(const std::string& authorized_entity,
                             const std::string& scope,
                             DeleteTokenCallback callback) {
  // Tokens with GCM scope act as Google Cloud Messaging registrations, so may
  // have associated encryption information in the GCMKeyStore. This needs to be
  // cleared when the token is deleted.
  DeleteTokenCallback wrapped_callback =
      scope == kGCMScope
          ? base::BindOnce(&InstanceID::DidDelete,
                           weak_ptr_factory_.GetWeakPtr(), authorized_entity,
                           std::move(callback))
          : std::move(callback);
  DeleteTokenImpl(authorized_entity, scope, std::move(wrapped_callback));
}

void InstanceID::DeleteID(DeleteIDCallback callback) {
  // Use "*" as authorized_entity to remove any encryption info for all tokens.
  DeleteIDImpl(
      base::BindOnce(&InstanceID::DidDelete, weak_ptr_factory_.GetWeakPtr(),
                     "*" /* authorized_entity */, std::move(callback)));
}

void InstanceID::DidDelete(const std::string& authorized_entity,
                           base::OnceCallback<void(Result result)> callback,
                           Result result) {
  gcm_driver_->GetEncryptionProviderInternal()->RemoveEncryptionInfo(
      app_id_, authorized_entity, base::BindOnce(std::move(callback), result));
}

}  // namespace instance_id
