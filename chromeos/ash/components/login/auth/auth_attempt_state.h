// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_ATTEMPT_STATE_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_ATTEMPT_STATE_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

// Tracks the state associated with a single attempt to log in to chromium OS.
// Enforces that methods are only called on the UI thread.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) AuthAttemptState {
 public:
  // Used to initialize for a login attempt.
  explicit AuthAttemptState(std::unique_ptr<UserContext> user_context);

  AuthAttemptState(const AuthAttemptState&) = delete;
  AuthAttemptState& operator=(const AuthAttemptState&) = delete;

  virtual ~AuthAttemptState();

  // Deprecated legacy method, will be removed soon.
  // Must be called from the UI thread.
  void RecordOnlineLoginComplete();

  // Copy |username_hash| into this object, so we can have
  // a copy we're sure to own, and can make available on the UI thread.
  // Must be called from the UI thread.
  void RecordUsernameHash(const std::string& username_hash);

  // Marks that the username hash request attempt has failed.
  void RecordUsernameHashFailed();

  // Marks username hash as being requested so that flow will block till both
  // requests (Mount/GetUsernameHash) are completed.
  void UsernameHashRequested();

  // Copy |cryptohome_code| into this object, so we can have a copy we're sure
  // to own, and can make available on the UI thread.
  // Must be called from the UI thread.
  void RecordCryptohomeStatus(cryptohome::MountError cryptohome_code);

  // Blow away locally stored cryptohome login status.
  // Must be called from the UI thread.
  void ResetCryptohomeStatus();

  virtual bool online_complete();

  virtual bool cryptohome_complete();
  virtual cryptohome::MountError cryptohome_code();

  virtual bool username_hash_obtained();
  virtual bool username_hash_valid();

  // Saved so we can retry client login, and also so we know for whom login
  // has succeeded, in the event of successful completion.
  std::unique_ptr<UserContext> user_context;

 protected:
  // Status of our online login attempt.
  bool online_complete_ = false;

  // Status of our cryptohome op attempt. Can only have one in flight at a time.
  bool cryptohome_complete_ = false;
  cryptohome::MountError cryptohome_code_ = cryptohome::MOUNT_ERROR_NONE;

 private:
  // Status of the crypthome GetSanitizedUsername() async call.
  // This gets initialized as being completed and those callers
  // that would explicitly request username hash would have to reset this.
  bool username_hash_obtained_ = true;

  // After the username hash request is completed, this marks whether
  // the request was successful.
  bool username_hash_valid_ = true;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_ATTEMPT_STATE_H_
