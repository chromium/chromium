// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_ui.h"

#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace password_manager::ui {

std::string StateToString(State state) {
  std::string_view state_name = "UNKNOWN_STATE";
  switch (state) {
    case INACTIVE_STATE:
      state_name = "INACTIVE_STATE";
      break;
    case PENDING_PASSWORD_STATE:
      state_name = "PENDING_PASSWORD_STATE";
      break;
    case SAVE_CONFIRMATION_STATE:
      state_name = "SAVE_CONFIRMATION_STATE";
      break;
    case UPDATE_CONFIRMATION_STATE:
      state_name = "UPDATE_CONFIRMATION_STATE";
      break;
    case MANAGE_STATE:
      state_name = "MANAGE_STATE";
      break;
    case CREDENTIAL_REQUEST_STATE:
      state_name = "CREDENTIAL_REQUEST_STATE";
      break;
    case AUTO_SIGNIN_STATE:
      state_name = "AUTO_SIGNIN_STATE";
      break;
    case PENDING_PASSWORD_UPDATE_STATE:
      state_name = "PENDING_PASSWORD_UPDATE_STATE";
      break;
    case MOVE_CREDENTIAL_AFTER_LOG_IN_STATE:
      state_name = "MOVE_CREDENTIAL_AFTER_LOG_IN_STATE";
      break;
    case PASSWORD_UPDATED_SAFE_STATE:
      state_name = "PASSWORD_UPDATED_SAFE_STATE";
      break;
    case PASSWORD_UPDATED_MORE_TO_FIX:
      state_name = "PASSWORD_UPDATED_MORE_TO_FIX";
      break;
    case BIOMETRIC_AUTHENTICATION_FOR_FILLING_STATE:
      state_name = "BIOMETRIC_AUTHENTICATION_FOR_FILLING_STATE";
      break;
    case BIOMETRIC_AUTHENTICATION_CONFIRMATION_STATE:
      state_name = "BIOMETRIC_AUTHENTICATION_CONFIRMATION_STATE";
      break;
    case GENERATED_PASSWORD_CONFIRMATION_STATE:
      state_name = "GENERATED_PASSWORD_CONFIRMATION_STATE";
      break;
    case KEYCHAIN_ERROR_STATE:
      state_name = "KEYCHAIN_ERROR_STATE";
      break;
    case NOTIFY_RECEIVED_SHARED_CREDENTIALS:
      state_name = "NOTIFY_RECEIVED_SHARED_CREDENTIALS";
      break;
    case MOVE_CREDENTIAL_FROM_MANAGE_BUBBLE_STATE:
      state_name = "MOVE_CREDENTIAL_FROM_MANAGE_BUBBLE_STATE";
      break;
    case PASSKEY_SAVED_CONFIRMATION_STATE:
      state_name = "PASSKEY_SAVED_CONFIRMATION_STATE";
      break;
    case PASSKEY_DELETED_CONFIRMATION_STATE:
      state_name = "PASSKEY_DELETED_CONFIRMATION_STATE";
      break;
    case PASSKEY_UPDATED_CONFIRMATION_STATE:
      state_name = "PASSKEY_UPDATED_CONFIRMATION_STATE";
      break;
    case PASSKEY_NOT_ACCEPTED_STATE:
      state_name = "PASSKEY_NOT_ACCEPTED_STATE";
      break;
    case PASSKEY_UPGRADE_STATE:
      state_name = "PASSKEY_UPGRADE_STATE";
      break;
    case PASSWORD_CHANGE_STATE:
      state_name = "PASSWORD_CHANGE_STATE";
      break;
  }
  return absl::StrFormat("%s (%d)", state_name, static_cast<int>(state));
}

}  // namespace password_manager::ui
