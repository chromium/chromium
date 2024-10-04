// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_session_authenticator.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/error_types.h"
#include "chromeos/ash/components/cryptohome/error_util.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/login/auth/cryptohome_parameter_utils.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/operation_chain_runner.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_directory_integrity_manager.h"
#include "components/user_manager/user_names.h"

namespace ash {

namespace {

std::unique_ptr<UserContext> RecordConfiguredFactors(
    std::unique_ptr<UserContext> context) {
  AuthEventsRecorder::Get()->RecordSessionAuthFactors(
      context->GetAuthFactorsData());
  return context;
}

}  // namespace

AuthSessionAuthenticator::AuthSessionAuthenticator(
    AuthStatusConsumer* consumer,
    std::unique_ptr<SafeModeDelegate> safe_mode_delegate,
    base::RepeatingCallback<void(const AccountId&)> user_recorder,
    bool new_user_can_become_owner,
    PrefService* local_state)
    : Authenticator(consumer),
      user_recorder_(std::move(user_recorder)),
      safe_mode_delegate_(std::move(safe_mode_delegate)),
      auth_factor_editor_(
          std::make_unique<AuthFactorEditor>(UserDataAuthClient::Get())),
      auth_performer_(
          std::make_unique<AuthPerformer>(UserDataAuthClient::Get(),
                                          base::DefaultClock::GetInstance())),
      mount_performer_(std::make_unique<MountPerformer>()),
      local_state_(local_state),
      new_user_can_become_owner_(new_user_can_become_owner) {
  DCHECK(safe_mode_delegate_);
  DCHECK(!user_recorder_.is_null());
}

AuthSessionAuthenticator::~AuthSessionAuthenticator() = default;

// Completes online authentication:
// *  User is likely to be new
// *  Provided password is assumed to be just verified by online flow
// This method is also called in case of password change detection if user
// decides to remove old cryptohome and start anew, which can only happen
// as a result of prior CompleteLogin call.
void AuthSessionAuthenticator::CompleteLogin(
    bool ephemeral,
    std::unique_ptr<UserContext> user_context) {
  PrepareForNewAttempt("CompleteLogin", "Regular user after online sign-in");
  CompleteLoginImpl(ephemeral, std::move(user_context));
}

// Implementation part, called by CompleteLogin.
void AuthSessionAuthenticator::CompleteLoginImpl(
    bool ephemeral,
    std::unique_ptr<UserContext> context) {
  DCHECK(context);
  DCHECK(context->GetUserType() == user_manager::UserType::kRegular ||
         context->GetUserType() == user_manager::UserType::kChild);
  // For now we don't support empty passwords:
  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    bool has_knowledge_factor = !context->GetKey()->GetSecret().empty();
    bool challenge_response_auth = !context->GetChallengeResponseKeys().empty();
    if (!has_knowledge_factor && !challenge_response_auth) {
      // TODO(crbug.com/40225479): Restore non-empty password check.
      LOGIN_LOG(ERROR) << "Empty password used in AuthenticateToLogin";
    }
  }
  StartAuthSessionForLogin(
      ephemeral, std::move(context), AuthSessionIntent::kDecrypt,
      base::BindOnce(&AuthSessionAuthenticator::DoCompleteLogin,
                     weak_factory_.GetWeakPtr(), ephemeral));
}

void AuthSessionAuthenticator::StartAuthSessionForLogin(
    bool ephemeral,
    std::unique_ptr<UserContext> context,
    AuthSessionIntent intent,
    StartAuthSessionCallback callback) {
  // Clone the context to be able to retry the StartAuthSession operation in
  // case we need to go through stale data removal.
  auto original_context = std::make_unique<UserContext>(*context);
  auth_performer_->StartAuthSession(
      std::move(context), ephemeral, intent,
      base::BindOnce(&AuthSessionAuthenticator::OnStartAuthSessionForLogin,
                     weak_factory_.GetWeakPtr(), ephemeral,
                     std::move(original_context), intent, std::move(callback)));
}

void AuthSessionAuthenticator::OnStartAuthSessionForLogin(
    bool ephemeral,
    std::unique_ptr<UserContext> original_context,
    AuthSessionIntent intent,
    StartAuthSessionCallback callback,
    bool user_exists,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    std::move(callback).Run(/*user_exists=*/false, std::move(context),
                            error.value());
    return;
  }
  if (user_exists && ephemeral) {
    // It's an edge case when cryptohomed didn't have a chance to delete the
    // stale data yet. Trigger the removal and retry.
    RemoveStaleUserForEphemeral(context->GetAuthSessionId(),
                                std::move(original_context), intent,
                                std::move(callback));
    return;
  }
  std::move(callback).Run(user_exists, std::move(context),
                          /*error=*/std::nullopt);
}

void AuthSessionAuthenticator::RemoveStaleUserForEphemeral(
    const std::string& auth_session_id,
    std::unique_ptr<UserContext> original_context,
    AuthSessionIntent intent,
    StartAuthSessionCallback callback) {
  if (auth_session_id.empty()) {
    NOTREACHED_IN_MIGRATION() << "Auth session should exist";
  }
  LOGIN_LOG(EVENT) << "Deleting stale ephemeral user";
  user_data_auth::RemoveRequest remove_request;
  remove_request.set_auth_session_id(auth_session_id);
  UserDataAuthClient::Get()->Remove(
      remove_request,
      base::BindOnce(&AuthSessionAuthenticator::OnRemoveStaleUserForEphemeral,
                     weak_factory_.GetWeakPtr(), std::move(original_context),
                     intent, std::move(callback)));
}

void AuthSessionAuthenticator::OnRemoveStaleUserForEphemeral(
    std::unique_ptr<UserContext> original_context,
    AuthSessionIntent intent,
    StartAuthSessionCallback callback,
    std::optional<user_data_auth::RemoveReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "Stale ephemeral user removal failed with error "
                     << error;
    std::move(callback).Run(/*user_exists=*/true, std::move(original_context),
                            AuthenticationError(error));
    return;
  }
  // Retry the auth session creation after we recovered from stale data.
  auth_performer_->StartAuthSession(
      std::move(original_context), /*ephemeral=*/true, intent,
      base::BindOnce(&AuthSessionAuthenticator::
                         OnStartAuthSessionForLoginAfterStaleRemoval,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AuthSessionAuthenticator::OnStartAuthSessionForLoginAfterStaleRemoval(
    StartAuthSessionCallback callback,
    bool user_exists,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    std::move(callback).Run(/*user_exists=*/false, std::move(context),
                            error.value());
    return;
  }
  if (user_exists) {
    // There's still stale ephemeral user despite the removal - abort.
    LOGIN_LOG(ERROR) << "Home directory exists for ephemeral user session";
    NotifyFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS, std::move(context));
    return;
  }
  std::move(callback).Run(user_exists, std::move(context),
                          /*error=*/std::nullopt);
}

void AuthSessionAuthenticator::StartAuthSessionForLoggedIn(
    bool ephemeral,
    std::unique_ptr<UserContext> context,
    AuthSessionIntent intent,
    StartAuthSessionCallback callback) {
  auth_performer_->StartAuthSession(std::move(context), ephemeral, intent,
                                    std::move(callback));
}

void AuthSessionAuthenticator::RecordCreatingNewUser(
    user_manager::UserDirectoryIntegrityManager::CleanupStrategy strategy,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  user_manager::UserDirectoryIntegrityManager integrity_manager(local_state_);
  integrity_manager.RecordCreatingNewUser(context->GetAccountId(), strategy);
  std::move(callback).Run(std::move(context),
                          /*authentication_error=*/std::nullopt);
}

void AuthSessionAuthenticator::RecordFirstAuthFactorAdded(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  user_manager::UserDirectoryIntegrityManager integrity_manager(local_state_);
  integrity_manager.ClearPrefs();
  std::move(callback).Run(std::move(context),
                          /*authentication_error=*/std::nullopt);
}

void AuthSessionAuthenticator::DoCompleteLogin(
    bool ephemeral,
    bool user_exists,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  AuthErrorCallback error_callback =
      base::BindOnce(&AuthSessionAuthenticator::ProcessCryptohomeError,
                     weak_factory_.GetWeakPtr(),
                     ephemeral ? AuthFailure::COULD_NOT_MOUNT_TMPFS
                               : AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Error starting authsession for Regular user "
                     << error.value().get_cryptohome_error();
    std::move(error_callback).Run(std::move(context), error.value());
    return;
  }
  DCHECK(!user_exists || !ephemeral);
  LOGIN_LOG(EVENT) << "Regular user CompleteLogin " << user_exists;
  const bool challenge_response_auth =
      !context->GetChallengeResponseKeys().empty();
  const bool has_password = !context->GetKey()->GetSecret().empty();
  std::vector<AuthOperation> steps;
  if (!user_exists) {
    if (safe_mode_delegate_->IsSafeMode()) {
      LOGIN_LOG(ERROR) << "New users are not allowed in safe mode";
      NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
      return;
    }

    if (ephemeral) {  // New ephemeral user
      steps.push_back(base::BindOnce(&MountPerformer::MountEphemeralDirectory,
                                     mount_performer_->AsWeakPtr()));
    } else {  // New persistent user
      using CleanupStrategy =
          user_manager::UserDirectoryIntegrityManager::CleanupStrategy;
      CleanupStrategy strategy = new_user_can_become_owner_
                                     ? CleanupStrategy::kSilentPowerwash
                                     : CleanupStrategy::kRemoveUser;
      bool ignore_owner_in_tests =
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              ash::switches::kCryptohomeIgnoreCleanupOwnershipForTesting);

      if (ignore_owner_in_tests &&
          strategy == CleanupStrategy::kSilentPowerwash) {
        LOG(WARNING) << "Overriding cleanup strategy due to testing";
        strategy = CleanupStrategy::kRemoveUser;
      }

      steps.push_back(
          base::BindOnce(&AuthSessionAuthenticator::RecordCreatingNewUser,
                         weak_factory_.GetWeakPtr(), strategy));
      // We need to store a user information as it would be used by
      // CryptohomeKeyDelegateServiceProvider and MisconfiguredUserCleaner
      // If the user creation process is interrupted, the known user record
      // will be cleared on reboot in
      // `UserDirectoryIntegrityManager::RemoveUser` via `UserManager`.
      steps.push_back(base::BindOnce(&AuthSessionAuthenticator::SaveKnownUser,
                                     weak_factory_.GetWeakPtr()));
      steps.push_back(base::BindOnce(&MountPerformer::CreateNewUser,
                                     mount_performer_->AsWeakPtr()));
      steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                     mount_performer_->AsWeakPtr()));
    }
    // In both cases, add a key
    if (challenge_response_auth) {
      steps.push_back(
          base::BindOnce(&AuthFactorEditor::AddContextChallengeResponseKey,
                         auth_factor_editor_->AsWeakPtr()));
      steps.push_back(
          base::BindOnce(&AuthSessionAuthenticator::RecordFirstAuthFactorAdded,
                         weak_factory_.GetWeakPtr()));
    } else {
      if (ash::switches::AreEmptyPasswordsAllowedForForTesting()) {
        // Empty passwords are currently not supported in ChromeOS, and
        // upcoming work on local passwords would significantly change code
        // behavior if empty password is used during initial login.
        // Some older tests might still use empty string as a password.
        // Such tests should be fixed by owners to use non-empty passwords.
        // If such fix requires non-trivial changes, the following flag
        // can be used as a short-term solution:
        // `--allow-empty-passwords-in-tests`
        steps.push_back(
            base::BindOnce(&AuthFactorEditor::AddContextKnowledgeKey,
                           auth_factor_editor_->AsWeakPtr()));
        steps.push_back(base::BindOnce(
            &AuthSessionAuthenticator::RecordFirstAuthFactorAdded,
            weak_factory_.GetWeakPtr()));
      } else if (ephemeral) {
        // Short-terms fix for b/344603210:
        // Ephemeral users don't have active authsession in onboarding
        // flow, so we need to set up their password here, if they have one.
        if (has_password) {
          steps.push_back(
              base::BindOnce(&AuthFactorEditor::AddContextKnowledgeKey,
                             auth_factor_editor_->AsWeakPtr()));
        }
      } else {
        // If Local passwords are enabled, password setup would
        // happen later in OOBE flow.
      }
    }  // challenge-response
  } else {  // existing user
    if (!challenge_response_auth) {
      // Password-based login
      const auto& factors = context->GetAuthFactorsData();
      if (!factors.FindOnlinePasswordFactor()) {
        // User has knowledge factor other than online password need
        // to go through custom flow.
        NotifyOnlinePasswordUnusable(std::move(context),
                                     /*online_password_mismatch=*/false);
        return;
      }

      // We are sure that password is correct, so intercept authentication
      // failure events and treat them as password change signals.
      error_callback = base::BindOnce(
          &AuthSessionAuthenticator::HandlePasswordChangeDetected,
          weak_factory_.GetWeakPtr(), std::move(error_callback));
    }
    // Existing users might require encryption migration: intercept related
    // error codes as well.
    error_callback =
        base::BindOnce(&AuthSessionAuthenticator::HandleMigrationRequired,
                       weak_factory_.GetWeakPtr(), std::move(error_callback));

    if (challenge_response_auth) {
      steps.push_back(
          base::BindOnce(&AuthPerformer::AuthenticateUsingChallengeResponseKey,
                         auth_performer_->AsWeakPtr()));
    } else {
      steps.push_back(
          base::BindOnce(&AuthPerformer::AuthenticateUsingKnowledgeKey,
                         auth_performer_->AsWeakPtr()));
    }
    steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                   mount_performer_->AsWeakPtr()));
    if (safe_mode_delegate_->IsSafeMode()) {
      steps.push_back(
          base::BindOnce(&AuthSessionAuthenticator::CheckOwnershipOperation,
                         weak_factory_.GetWeakPtr()));
    }
  }
  // In addition to factors suitable for authentication, fetch a set of
  // supported factor for users.
  steps.push_back(base::BindOnce(&AuthFactorEditor::GetAuthFactorsConfiguration,
                                 auth_factor_editor_->AsWeakPtr()));
  AuthSuccessCallback success_callback = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());

  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

// Authentication from user pod.
// *  User could mistype their password/PIN.
// *  User homedir is expected to exist
void AuthSessionAuthenticator::AuthenticateToLogin(
    bool ephemeral,
    std::unique_ptr<UserContext> context) {
  DCHECK(context);
  DCHECK(context->GetUserType() == user_manager::UserType::kRegular ||
         context->GetUserType() == user_manager::UserType::kChild ||
         context->GetUserType() == user_manager::UserType::kPublicAccount);
  PrepareForNewAttempt("AuthenticateToLogin", "Returning regular user");

  bool challenge_response_auth = !context->GetChallengeResponseKeys().empty();

  // For now we don't support empty passwords:
  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    if (context->GetKey()->GetSecret().empty() && !challenge_response_auth) {
      // TODO(crbug.com/40225479): Restore non-empty password check.
      LOGIN_LOG(ERROR) << "Empty password used in AuthenticateToLogin";
    }
  }
  StartAuthSessionForLogin(
      ephemeral, std::move(context), AuthSessionIntent::kDecrypt,
      base::BindOnce(&AuthSessionAuthenticator::DoLoginAsExistingUser,
                     weak_factory_.GetWeakPtr(), ephemeral));
}

void AuthSessionAuthenticator::AuthenticateToUnlock(
    bool ephemeral,
    std::unique_ptr<UserContext> user_context) {
  DCHECK(user_context);
  DCHECK(user_context->GetUserType() == user_manager::UserType::kRegular ||
         user_context->GetUserType() == user_manager::UserType::kChild ||
         user_context->GetUserType() == user_manager::UserType::kPublicAccount);
  PrepareForNewAttempt("AuthenticateToUnlock", "Returning regular user");

  bool challenge_response_auth =
      !user_context->GetChallengeResponseKeys().empty();

  // For now we don't support empty passwords:
  if (user_context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    if (user_context->GetKey()->GetSecret().empty() &&
        !challenge_response_auth) {
      // TODO(crbug.com/40225479): Restore non-empty password check.
      LOGIN_LOG(ERROR) << "Empty password used in AuthenticateToLogin";
    }
  }

  AuthSessionIntent intent = AuthSessionIntent::kVerifyOnly;

  StartAuthSessionForLoggedIn(
      ephemeral, std::move(user_context), intent,
      base::BindOnce(&AuthSessionAuthenticator::DoUnlock,
                     weak_factory_.GetWeakPtr(), ephemeral));
}

void AuthSessionAuthenticator::DoLoginAsExistingUser(
    bool ephemeral,
    bool user_exists,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  AuthErrorCallback error_callback =
      base::BindOnce(&AuthSessionAuthenticator::ProcessCryptohomeError,
                     weak_factory_.GetWeakPtr(),
                     ephemeral ? AuthFailure::COULD_NOT_MOUNT_TMPFS
                               : AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Error starting authsession for Regular user "
                     << error.value().get_cryptohome_error();
    std::move(error_callback).Run(std::move(context), error.value());
    return;
  }
  LOGIN_LOG(EVENT) << "Regular user login " << user_exists;

  if (!user_exists) {  // Should not happen
    LOGIN_LOG(ERROR)
        << "User directory does not exist for supposedly existing user";
    std::move(error_callback)
        .Run(std::move(context),
             AuthenticationError{
                 cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                     user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND)});
    return;
  }
  DCHECK(user_exists && !ephemeral);

  bool challenge_response_auth = !context->GetChallengeResponseKeys().empty();

  AuthSuccessCallback success_callback =
      base::BindOnce(&RecordConfiguredFactors)
          .Then(base::BindOnce(&AuthSessionAuthenticator::NotifyAuthSuccess,
                               weak_factory_.GetWeakPtr()));

  // Existing users might require encryption migration: intercept related
  // error codes.
  error_callback =
      base::BindOnce(&AuthSessionAuthenticator::HandleMigrationRequired,
                     weak_factory_.GetWeakPtr(), std::move(error_callback));

  std::vector<AuthOperation> steps;
  if (challenge_response_auth) {
    steps.push_back(
        base::BindOnce(&AuthPerformer::AuthenticateUsingChallengeResponseKey,
                       auth_performer_->AsWeakPtr()));
  } else {
    steps.push_back(
        base::BindOnce(&AuthPerformer::AuthenticateUsingKnowledgeKey,
                       auth_performer_->AsWeakPtr()));
  }
  steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                 mount_performer_->AsWeakPtr()));
  if (safe_mode_delegate_->IsSafeMode()) {
    steps.push_back(
        base::BindOnce(&AuthSessionAuthenticator::CheckOwnershipOperation,
                       weak_factory_.GetWeakPtr()));
  }
  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

void AuthSessionAuthenticator::DoUnlock(
    bool ephemeral,
    bool user_exists,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  AuthErrorCallback error_callback =
      base::BindOnce(&AuthSessionAuthenticator::ProcessCryptohomeError,
                     weak_factory_.GetWeakPtr(), AuthFailure::UNLOCK_FAILED);
  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Error starting authsession for Regular user for "
                        "verification intent "
                     << error.value().get_cryptohome_error();
    std::move(error_callback).Run(std::move(context), error.value());
    return;
  }

  LOGIN_LOG(EVENT) << "Regular User Unlock " << user_exists << " " << ephemeral;

  if (!user_exists && !ephemeral) {
    LOGIN_LOG(ERROR)
        << "User directory does not exist for supposedly existing user";
    std::move(error_callback)
        .Run(std::move(context),
             AuthenticationError{
                 cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                     user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND)});
    return;
  }
  DCHECK(user_exists || ephemeral);

  bool challenge_response_auth = !context->GetChallengeResponseKeys().empty();

  AuthSuccessCallback success_callback = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());

  std::vector<AuthOperation> steps;
  if (challenge_response_auth) {
    steps.push_back(
        base::BindOnce(&AuthPerformer::AuthenticateUsingChallengeResponseKey,
                       auth_performer_->AsWeakPtr()));
  } else {
    steps.push_back(
        base::BindOnce(&AuthPerformer::AuthenticateUsingKnowledgeKey,
                       auth_performer_->AsWeakPtr()));
  }

  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

void AuthSessionAuthenticator::LoginOffTheRecord() {
  PrepareForNewAttempt("LoginOffTheRecord", "Guest login");

  std::unique_ptr<UserContext> context = std::make_unique<UserContext>(
      user_manager::UserType::kGuest, user_manager::GuestAccountId());

  // Guest can not be be an owner.
  if (safe_mode_delegate_->IsSafeMode()) {
    LOGIN_LOG(EVENT) << "Guest can not sign-in in safe mode";
    NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
    return;
  }

  AuthErrorCallback error_callback = base::BindOnce(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(), AuthFailure::COULD_NOT_MOUNT_TMPFS);

  AuthSuccessCallback success_callback =
      base::BindOnce(&AuthSessionAuthenticator::NotifyGuestSuccess,
                     weak_factory_.GetWeakPtr());

  std::vector<AuthOperation> steps;
  steps.push_back(base::BindOnce(&MountPerformer::MountGuestDirectory,
                                 mount_performer_->AsWeakPtr()));
  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

// Public sessions aka Managed Guest Sessions are always ephemeral.
// Most of the MGS have no credentials, but it optionally can
// have a password set by extension (so that it is possible to lock session).
void AuthSessionAuthenticator::LoginAsPublicSession(
    const UserContext& user_context) {
  DCHECK_EQ(user_context.GetUserType(), user_manager::UserType::kPublicAccount);

  PrepareForNewAttempt("LoginAsPublicSession", "Managed guest session");

  std::unique_ptr<UserContext> context =
      std::make_unique<UserContext>(user_context);

  if (safe_mode_delegate_->IsSafeMode()) {
    LOGIN_LOG(EVENT) << "Managed guests can not sign-in in safe mode";
    NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
    return;
  }

  StartAuthSessionForLogin(
      /*ephemeral=*/true, std::move(context), AuthSessionIntent::kDecrypt,
      base::BindOnce(&AuthSessionAuthenticator::DoLoginAsPublicSession,
                     weak_factory_.GetWeakPtr()));
}

void AuthSessionAuthenticator::DoLoginAsPublicSession(
    bool user_exists,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  AuthErrorCallback error_callback = base::BindOnce(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(), AuthFailure::COULD_NOT_MOUNT_TMPFS);

  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Error starting authsession for MGS "
                     << error.value().get_cryptohome_error();
    std::move(error_callback).Run(std::move(context), error.value());
    return;
  }
  if (user_exists) {  // Should not happen
    LOGIN_LOG(ERROR) << "Home directory exists for MGS";
    NotifyFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS, std::move(context));
    return;
  }
  AuthSuccessCallback success_callback = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());

  std::vector<AuthOperation> steps;
  steps.push_back(base::BindOnce(&MountPerformer::MountEphemeralDirectory,
                                 mount_performer_->AsWeakPtr()));
  if (context->GetKey()->GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN ||
      !context->GetKey()->GetSecret().empty()) {
    steps.push_back(base::BindOnce(&AuthFactorEditor::AddContextKnowledgeKey,
                                   auth_factor_editor_->AsWeakPtr()));
  }

  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

void AuthSessionAuthenticator::LoginAsKioskAccount(
    const AccountId& app_account_id,
    bool ephemeral) {
  LoginAsKioskImpl(app_account_id, user_manager::UserType::kKioskApp,
                   /*force_dircrypto=*/false, /*ephemeral=*/ephemeral);
}

void AuthSessionAuthenticator::LoginAsWebKioskAccount(
    const AccountId& app_account_id,
    bool ephemeral) {
  LoginAsKioskImpl(app_account_id, user_manager::UserType::kWebKioskApp,
                   /*force_dircrypto=*/false, /*ephemeral=*/ephemeral);
}

void AuthSessionAuthenticator::LoginAsIwaKioskAccount(
    const AccountId& app_account_id,
    bool ephemeral) {
  LoginAsKioskImpl(app_account_id, user_manager::UserType::kKioskIWA,
                   /*force_dircrypto=*/false, /*ephemeral=*/ephemeral);
}

void AuthSessionAuthenticator::LoginAsKioskImpl(
    const AccountId& app_account_id,
    user_manager::UserType user_type,
    bool force_dircrypto,
    bool ephemeral) {
  PrepareForNewAttempt("LoginAs*Kiosk", "Kiosk user");

  std::unique_ptr<UserContext> context =
      std::make_unique<UserContext>(user_type, app_account_id);
  context->SetIsForcingDircrypto(force_dircrypto);

  // Kiosk can not be an owner.
  if (safe_mode_delegate_->IsSafeMode()) {
    LOGIN_LOG(EVENT) << "Kiosks can not sign-in in safe mode";
    NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
    return;
  }
  StartAuthSessionForLogin(
      ephemeral, std::move(context), AuthSessionIntent::kDecrypt,
      base::BindOnce(&AuthSessionAuthenticator::DoLoginAsKiosk,
                     weak_factory_.GetWeakPtr(), ephemeral));
}

void AuthSessionAuthenticator::DoLoginAsKiosk(
    bool ephemeral,
    bool user_exists,
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  AuthErrorCallback error_callback =
      base::BindOnce(&AuthSessionAuthenticator::ProcessCryptohomeError,
                     weak_factory_.GetWeakPtr(),
                     ephemeral ? AuthFailure::COULD_NOT_MOUNT_TMPFS
                               : AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
  if (error.has_value()) {
    LOGIN_LOG(ERROR) << "Error starting authsession for Kiosk "
                     << error.value().get_cryptohome_error();
    std::move(error_callback).Run(std::move(context), error.value());
    return;
  }
  LOGIN_LOG(EVENT) << "Kiosk user " << user_exists;
  DCHECK(!user_exists || !ephemeral);
  AuthSuccessCallback success_callback = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());

  std::vector<AuthOperation> steps;
  if (user_exists) {
    // Existing Kiosks might require encryption migration: intercept related
    // error codes.
    error_callback =
        base::BindOnce(&AuthSessionAuthenticator::HandleMigrationRequired,
                       weak_factory_.GetWeakPtr(), std::move(error_callback));

    steps.push_back(base::BindOnce(&AuthPerformer::AuthenticateAsKiosk,
                                   auth_performer_->AsWeakPtr()));
    steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                   mount_performer_->AsWeakPtr()));
  } else if (ephemeral) {
    steps.push_back(base::BindOnce(&MountPerformer::MountEphemeralDirectory,
                                   mount_performer_->AsWeakPtr()));
  } else {
    steps.push_back(
        base::BindOnce(&AuthSessionAuthenticator::RecordCreatingNewUser,
                       weak_factory_.GetWeakPtr(),
                       user_manager::UserDirectoryIntegrityManager::
                           CleanupStrategy::kRemoveUser));
    steps.push_back(base::BindOnce(&MountPerformer::CreateNewUser,
                                   mount_performer_->AsWeakPtr()));
    steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                   mount_performer_->AsWeakPtr()));
    steps.push_back(base::BindOnce(&AuthFactorEditor::AddKioskKey,
                                   auth_factor_editor_->AsWeakPtr()));
    steps.push_back(
        base::BindOnce(&AuthSessionAuthenticator::RecordFirstAuthFactorAdded,
                       weak_factory_.GetWeakPtr()));
  }
  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

void AuthSessionAuthenticator::LoginAuthenticated(
    std::unique_ptr<UserContext> context) {
  AuthErrorCallback error_callback = base::BindOnce(
      &AuthSessionAuthenticator::ProcessCryptohomeError,
      weak_factory_.GetWeakPtr(), AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);

  AuthSuccessCallback success_callback = base::BindOnce(
      &AuthSessionAuthenticator::NotifyAuthSuccess, weak_factory_.GetWeakPtr());
  std::vector<AuthOperation> steps;
  steps.push_back(base::BindOnce(&MountPerformer::MountPersistentDirectory,
                                 mount_performer_->AsWeakPtr()));

  RunOperationChain(std::move(context), std::move(steps),
                    std::move(success_callback), std::move(error_callback));
}

void AuthSessionAuthenticator::OnAuthSuccess() {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::OnAuthFailure(const AuthFailure& error) {
  NOTIMPLEMENTED();
}

void AuthSessionAuthenticator::PrepareForNewAttempt(
    const std::string& method_id,
    const std::string& long_desc) {
  LOGIN_LOG(USER) << "Authentication attempt : " << long_desc;
  VLOG(1) << "AuthSessionAuthenticator::" << method_id;

  // Assume no ongoing authentication requests happen at the moment.
  DCHECK(!weak_factory_.HasWeakPtrs());
  // Clear all ongoing requests
  auth_factor_editor_->InvalidateCurrentAttempts();
  auth_performer_->InvalidateCurrentAttempts();
  mount_performer_->InvalidateCurrentAttempts();
  weak_factory_.InvalidateWeakPtrs();
}

bool AuthSessionAuthenticator::ResolveCryptohomeError(
    AuthFailure::FailureReason default_error,
    AuthenticationError& error) {
  DCHECK_EQ(error.get_origin(), AuthenticationError::Origin::kCryptohome);
  cryptohome::ErrorWrapper error_wrapper = error.get_cryptohome_error();
  if (
      // Not an error:
      ErrorMatches(error_wrapper, user_data_auth::CRYPTOHOME_ERROR_NOT_SET) ||
      // Some errors need to be handled explicitly and can not be resolved to
      // AuthFailure:
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::
                       CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_REMOVE_CREDENTIALS_FAILED) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_UPDATE_CREDENTIALS_FAILED) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_RECOVERY_TRANSIENT) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_RECOVERY_FATAL) ||

      // Fatal errors that can not be handled gracefully:
      ErrorMatches(
          error_wrapper,
          user_data_auth::CRYPTOHOME_ERROR_LOCKBOX_SIGNATURE_INVALID) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_LOCKBOX_CANNOT_SIGN) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_BOOT_ATTRIBUTE_NOT_FOUND) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::CRYPTOHOME_ERROR_BOOT_ATTRIBUTES_CANNOT_SIGN) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_TPM_EK_NOT_AVAILABLE) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_ATTESTATION_NOT_READY) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_CANNOT_CONNECT_TO_CA) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_CA_REFUSED_ENROLLMENT) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_CA_REFUSED_CERTIFICATE) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::CRYPTOHOME_ERROR_INTERNAL_ATTESTATION_ERROR) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::
              CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_INVALID) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::
              CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_CANNOT_STORE) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::
              CRYPTOHOME_ERROR_FIRMWARE_MANAGEMENT_PARAMETERS_CANNOT_REMOVE) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::
              CRYPTOHOME_ERROR_UPDATE_USER_ACTIVITY_TIMESTAMP_FAILED) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_FAILED_TO_EXTEND_PCR) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_FAILED_TO_READ_PCR) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_PCR_ALREADY_EXTENDED) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::CRYPTOHOME_ERROR_FIDO_MAKE_CREDENTIAL_FAILED) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::CRYPTOHOME_ERROR_FIDO_GET_ASSERTION_FAILED)) {
    return false;
  }

  if (ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND)) {
    error.ResolveToFailure(AuthFailure::MISSING_CRYPTOHOME);
    return true;
  }

  if (ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_NOT_IMPLEMENTED) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_INVALID_ARGUMENT) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_TOKEN_SERIALIZATION_FAILED)) {
    // Fatal implementation errors
    error.ResolveToFailure(default_error);
    return true;
  }

  if (ErrorMatches(
          error_wrapper,
          user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_ERROR_INTERNAL) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_RETRY_REQUIRED) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_DENIED)) {
    // Fingerprint errors
    error.ResolveToFailure(default_error);
    return true;
  }

  if (ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_MOUNT_FATAL) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_KEY_QUOTA_EXCEEDED) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_BACKING_STORE_FAILURE) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::
                       CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_FINALIZE_FAILED) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_GET_FAILED) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::CRYPTOHOME_ERROR_INSTALL_ATTRIBUTES_SET_FAILED)) {
    // Fatal system state errors
    error.ResolveToFailure(default_error);
    return true;
  }

  if (ErrorMatches(
          error_wrapper,
          user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_MIGRATE_KEY_FAILED) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED) ||

      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_DENIED) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_KEY_LABEL_EXISTS) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_UPDATE_SIGNATURE_INVALID) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_UNKNOWN_LEGACY)) {
    // Assumptions about key are not correct
    error.ResolveToFailure(default_error);
    return true;
  }

  if (ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::CRYPTOHOME_ERROR_UNAUTHENTICATED_AUTH_SESSION)) {
    // Auth session expired, might need to handle it separately later.
    error.ResolveToFailure(default_error);
    return true;
  }

  if (ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_TPM_COMM_ERROR) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_TPM_NEEDS_REBOOT)) {
    error.ResolveToFailure(AuthFailure::TPM_ERROR);
    return true;
  }

  if (ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_CREDENTIAL_LOCKED)) {
    // PIN is locked out, for now mark it as auth failure, and pin lockout
    // would be detected by PinStorageCryptohome.
    error.ResolveToFailure(default_error);
    return true;
  }

  if (ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_CREDENTIAL_EXPIRED)) {
    // TODO(b/285459974): Decide how to deal with credential expired error
    // from cryptohome.
    error.ResolveToFailure(default_error);
    return true;
  }

  if (ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY)) {
    // Assumption about system state is not correct
    error.ResolveToFailure(default_error);
    return true;
  }

  if (ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_REMOVE_FAILED)) {
    error.ResolveToFailure(AuthFailure::DATA_REMOVAL_FAILED);
    return true;
  }

  if (ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_TPM_UPDATE_REQUIRED)) {
    error.ResolveToFailure(AuthFailure::TPM_UPDATE_REQUIRED);
    return true;
  }

  if (ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_VAULT_UNRECOVERABLE) ||
      ErrorMatches(error_wrapper,
                   user_data_auth::CRYPTOHOME_ERROR_UNUSABLE_VAULT)) {
    error.ResolveToFailure(AuthFailure::UNRECOVERABLE_CRYPTOHOME);
    return true;
  }

  if (ErrorMatches(
          error_wrapper,
          user_data_auth::CryptohomeErrorCode_INT_MIN_SENTINEL_DO_NOT_USE_) ||
      ErrorMatches(
          error_wrapper,
          user_data_auth::CryptohomeErrorCode_INT_MAX_SENTINEL_DO_NOT_USE_)) {
    // Ignored
    return true;
  }

  // We need the default case here so that it is possible to add new
  // CryptohomeErrorCode, because CryptohomeErrorCode is defined in another
  // repo.
  // However, we should seek to handle all CryptohomeErrorCode and not let
  // any of them hit the default block.
  NOTREACHED_IN_MIGRATION()
      << "Unhandled CryptohomeError in ProcessCryptohomeError"
         ": "
      << error.get_cryptohome_error();
  return false;
}

void AuthSessionAuthenticator::ProcessCryptohomeError(
    AuthFailure::FailureReason default_error,
    std::unique_ptr<UserContext> context,
    AuthenticationError error) {
  if (!consumer_) {
    return;
  }
  DCHECK_EQ(error.get_origin(), AuthenticationError::Origin::kCryptohome);
  DCHECK(cryptohome::HasError(error.get_cryptohome_error()));

  if (cryptohome::ErrorMatches(
          error.get_cryptohome_error(),
          user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED)) {
    // for now treat it as login failed:
    error.ResolveToFailure(default_error);
    NotifyFailure(error.get_resolved_failure(), std::move(context));
    return;
  }
  bool handled = ResolveCryptohomeError(default_error, error);
  if (!handled) {
    NOTREACHED_IN_MIGRATION()
        << "Unhandled cryptohome error: " << error.get_cryptohome_error();
    SCOPED_CRASH_KEY_NUMBER("Cryptohome", "error_code",
                            error.get_cryptohome_error().code());
    base::debug::DumpWithoutCrashing();
    error.ResolveToFailure(default_error);
  }

  NotifyFailure(error.get_resolved_failure(), std::move(context));
}

void AuthSessionAuthenticator::HandlePasswordChangeDetected(
    AuthErrorCallback fallback,
    std::unique_ptr<UserContext> context,
    AuthenticationError error) {
  if (cryptohome::ErrorMatches(
          error.get_cryptohome_error(),
          user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED)) {
    LOGIN_LOG(EVENT) << "Password change detected";
    NotifyOnlinePasswordUnusable(std::move(context),
                                 /*online_password_mismatch=*/true);
    return;
  }
  std::move(fallback).Run(std::move(context), std::move(error));
}

void AuthSessionAuthenticator::NotifyOnlinePasswordUnusable(
    std::unique_ptr<UserContext> context,
    bool online_password_mismatch) {
  LOGIN_LOG(EVENT) << "Online password unusable / " << online_password_mismatch;
  if (!consumer_) {
    return;
  }
  consumer_->OnOnlinePasswordUnusable(std::move(context),
                                      online_password_mismatch);
}

void AuthSessionAuthenticator::HandleMigrationRequired(
    AuthErrorCallback fallback,
    std::unique_ptr<UserContext> context,
    AuthenticationError error) {
  const bool migration_required = cryptohome::ErrorMatches(
      error.get_cryptohome_error(),
      user_data_auth::CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION);
  const bool incomplete_migration = cryptohome::ErrorMatches(
      error.get_cryptohome_error(),
      user_data_auth::CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE);
  if (migration_required || incomplete_migration) {
    LOGIN_LOG(EVENT) << "Old encryption detected";
    if (!consumer_) {
      return;
    }
    consumer_->OnOldEncryptionDetected(std::move(context),
                                       incomplete_migration);
    return;
  }
  std::move(fallback).Run(std::move(context), std::move(error));
}

void AuthSessionAuthenticator::NotifyAuthSuccess(
    std::unique_ptr<UserContext> context) {
  LOGIN_LOG(EVENT) << "Logged in successfully";

  if (consumer_) {
    consumer_->OnAuthSuccess(*context);
  }
}

void AuthSessionAuthenticator::NotifyGuestSuccess(
    std::unique_ptr<UserContext> context) {
  LOGIN_LOG(EVENT) << "Logged in as guest";
  if (consumer_) {
    consumer_->OnOffTheRecordAuthSuccess();
  }
}

void AuthSessionAuthenticator::NotifyFailure(
    AuthFailure::FailureReason reason,
    std::unique_ptr<UserContext> context) {
  if (consumer_) {
    consumer_->OnAuthFailure(AuthFailure(reason));
  }
}

void AuthSessionAuthenticator::CheckOwnershipOperation(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  if (!safe_mode_delegate_->IsSafeMode()) {
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
  }
  LOGIN_LOG(EVENT) << "Running in safe mode";
  // Save value as context will be moved.
  auto user_hash = context->GetUserIDHash();
  // Device is running in the safe mode, need to check if user is an owner.
  safe_mode_delegate_->CheckSafeModeOwnership(
      user_hash,
      base::BindOnce(&AuthSessionAuthenticator::OnSafeModeOwnershipCheck,
                     weak_factory_.GetWeakPtr(), std::move(context),
                     std::move(callback)));
}

void AuthSessionAuthenticator::OnSafeModeOwnershipCheck(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    bool is_owner) {
  if (is_owner) {
    LOGIN_LOG(EVENT) << "Safe mode: owner";
    std::move(callback).Run(std::move(context), std::nullopt);
    return;
  }
  LOGIN_LOG(EVENT) << "Safe mode: non-owner";
  mount_performer_->UnmountDirectories(
      std::move(context),
      base::BindOnce(&AuthSessionAuthenticator::OnUnmountForNonOwner,
                     weak_factory_.GetWeakPtr()));
}

// Notify that owner is required upon success
// Crash if directory could not be unmounted
void AuthSessionAuthenticator::OnUnmountForNonOwner(
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  if (error) {
    // Crash if could not unmount home directory, and let session_manager
    // handle it.
    LOG(FATAL) << "Failed to unmount non-owner home directory "
               << error->get_cryptohome_error();
  } else {
    NotifyFailure(AuthFailure::OWNER_REQUIRED, std::move(context));
  }
}

void AuthSessionAuthenticator::SaveKnownUser(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  user_recorder_.Run(context->GetAccountId());
  std::move(callback).Run(std::move(context), std::nullopt);
}

}  // namespace ash
