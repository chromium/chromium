// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_MOCK_HOMEDIR_METHODS_H_
#define CHROMEOS_CRYPTOHOME_MOCK_HOMEDIR_METHODS_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cryptohome {

class COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME) MockHomedirMethods
    : public HomedirMethods {
 public:
  MockHomedirMethods();
  ~MockHomedirMethods() override;

  void SetUp(bool success, MountError return_code);

  MOCK_METHOD4(CheckKeyEx,
               void(const Identification& id,
                    const AuthorizationRequest& auth,
                    const CheckKeyRequest& request,
                    Callback callback));
  MOCK_METHOD4(AddKeyEx,
               void(const Identification& id,
                    const AuthorizationRequest& auth,
                    const AddKeyRequest& request,
                    Callback callback));
  MOCK_METHOD4(RemoveKeyEx,
               void(const Identification& id,
                    const AuthorizationRequest& auth,
                    const RemoveKeyRequest& request,
                    Callback callback));

 private:
  void DoCallback(Callback callback);
  void DoAddKeyCallback(Callback callback);

  bool success_ = false;
  MountError return_code_ = MOUNT_ERROR_NONE;

  DISALLOW_COPY_AND_ASSIGN(MockHomedirMethods);
};

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_MOCK_HOMEDIR_METHODS_H_
