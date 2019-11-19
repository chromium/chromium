// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/homedir_methods.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "components/device_event_log/device_event_log.h"

using google::protobuf::RepeatedPtrField;

namespace cryptohome {

namespace {

HomedirMethods* g_homedir_methods = NULL;

// The implementation of HomedirMethods
class HomedirMethodsImpl : public HomedirMethods {
 public:
  HomedirMethodsImpl() {}

  ~HomedirMethodsImpl() override = default;

  void CheckKeyEx(const Identification& id,
                  const cryptohome::AuthorizationRequest& auth,
                  const cryptohome::CheckKeyRequest& request,
                  const Callback& callback) override {
    chromeos::CryptohomeClient::Get()->CheckKeyEx(
        CreateAccountIdentifierFromIdentification(id), auth, request,
        base::BindOnce(&HomedirMethodsImpl::OnBaseReplyCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void AddKeyEx(const Identification& id,
                const AuthorizationRequest& auth,
                const AddKeyRequest& request,
                const Callback& callback) override {
    chromeos::CryptohomeClient::Get()->AddKeyEx(
        CreateAccountIdentifierFromIdentification(id), auth, request,
        base::BindOnce(&HomedirMethodsImpl::OnBaseReplyCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RemoveKeyEx(const Identification& id,
                   const AuthorizationRequest& auth,
                   const RemoveKeyRequest& request,
                   const Callback& callback) override {
    chromeos::CryptohomeClient::Get()->RemoveKeyEx(
        CreateAccountIdentifierFromIdentification(id), auth, request,
        base::BindOnce(&HomedirMethodsImpl::OnBaseReplyCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void UpdateKeyEx(const Identification& id,
                   const AuthorizationRequest& auth,
                   const UpdateKeyRequest& request,
                   const Callback& callback) override {
    chromeos::CryptohomeClient::Get()->UpdateKeyEx(
        CreateAccountIdentifierFromIdentification(id), auth, request,
        base::BindOnce(&HomedirMethodsImpl::OnBaseReplyCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void MassRemoveKeys(const Identification& id,
                      const AuthorizationRequest& auth,
                      const MassRemoveKeysRequest& request,
                      const Callback& callback) override {
    chromeos::CryptohomeClient::Get()->MassRemoveKeys(
        CreateAccountIdentifierFromIdentification(id), auth, request,
        base::BindOnce(&HomedirMethodsImpl::OnBaseReplyCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnBaseReplyCallback(const Callback& callback,
                           base::Optional<BaseReply> reply) {
    if (!reply.has_value()) {
      callback.Run(false, MOUNT_ERROR_FATAL);
      return;
    }
    if (reply->has_error() && reply->error() != CRYPTOHOME_ERROR_NOT_SET) {
      callback.Run(false, CryptohomeErrorToMountError(reply->error()));
      return;
    }
    callback.Run(true, MOUNT_ERROR_NONE);
  }

  base::WeakPtrFactory<HomedirMethodsImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HomedirMethodsImpl);
};

}  // namespace

// static
void HomedirMethods::Initialize() {
  if (g_homedir_methods) {
    LOG(WARNING) << "HomedirMethods was already initialized";
    return;
  }
  g_homedir_methods = new HomedirMethodsImpl();
  VLOG(1) << "HomedirMethods initialized";
}

// static
void HomedirMethods::InitializeForTesting(HomedirMethods* homedir_methods) {
  if (g_homedir_methods) {
    LOG(WARNING) << "HomedirMethods was already initialized";
    return;
  }
  g_homedir_methods = homedir_methods;
  VLOG(1) << "HomedirMethods initialized";
}

// static
void HomedirMethods::Shutdown() {
  if (!g_homedir_methods) {
    LOG(WARNING) << "AsyncMethodCaller::Shutdown() called with NULL manager";
    return;
  }
  delete g_homedir_methods;
  g_homedir_methods = NULL;
  VLOG(1) << "HomedirMethods Shutdown completed";
}

// static
HomedirMethods* HomedirMethods::GetInstance() { return g_homedir_methods; }

}  // namespace cryptohome
