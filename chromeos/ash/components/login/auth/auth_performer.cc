// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_performer.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/auth_factor_conversions.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/cryptohome/cryptohome_util.h"
#include "chromeos/ash/components/cryptohome/error_types.h"
#include "chromeos/ash/components/cryptohome/error_util.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/constants/cryptohome_key_delegate_constants.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/login/auth/challenge_response/key_label_utils.h"
#include "chromeos/ash/components/login/auth/cryptohome_parameter_utils.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/auth_session_status.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

bool IsKioskUserType(user_manager::UserType type) {
  return type == user_manager::USER_TYPE_KIOSK_APP ||
         type == user_manager::USER_TYPE_ARC_KIOSK_APP ||
         type == user_manager::USER_TYPE_WEB_KIOSK_APP;
}

user_data_auth::AuthIntent SerializeIntent(AuthSessionIntent intent) {
  switch (intent) {
    case AuthSessionIntent::kDecrypt:
      return user_data_auth::AUTH_INTENT_DECRYPT;
    case AuthSessionIntent::kVerifyOnly:
      return user_data_auth::AUTH_INTENT_VERIFY_ONLY;
    case AuthSessionIntent::kWebAuthn:
      return user_data_auth::AUTH_INTENT_WEBAUTHN;
  }
}

absl::optional<AuthSessionIntent> DeserializeIntent(
    user_data_auth::AuthIntent intent) {
  switch (intent) {
    case user_data_auth::AUTH_INTENT_DECRYPT:
      return AuthSessionIntent::kDecrypt;
    case user_data_auth::AUTH_INTENT_VERIFY_ONLY:
      return AuthSessionIntent::kVerifyOnly;
    case user_data_auth::AUTH_INTENT_WEBAUTHN:
      return AuthSessionIntent::kWebAuthn;
    default:
      NOTIMPLEMENTED() << "Other intents not implemented yet, intent: "
                       << intent;
  }
  return absl::nullopt;
}

}  // namespace

AuthPerformer::AuthPerformer(UserDataAuthClient* client,
                             const base::Clock* clock)
    : client_(client), clock_(clock) {
  CHECK(client_);
  CHECK(clock_);
}

AuthPerformer::~AuthPerformer() = default;

void AuthPerformer::InvalidateCurrentAttempts() {
  weak_factory_.InvalidateWeakPtrs();
}

base::WeakPtr<AuthPerformer> AuthPerformer::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
void AuthPerformer::FillAuthenticationData(
    const base::Time& reference_time,
    const user_data_auth::AuthSessionProperties& session_properties,
    UserContext& out_context) {
  DCHECK(session_properties.authorized_for_size() > 0);
  out_context.ClearAuthorizedIntents();
  for (const auto& authorized_for : session_properties.authorized_for()) {
    auto intent = DeserializeIntent(
        static_cast<user_data_auth::AuthIntent>(authorized_for));
    if (intent.has_value()) {
      out_context.AddAuthorizedIntent(intent.value());
    }
  }
  out_context.SetSessionLifetime(
      reference_time + base::Seconds(session_properties.seconds_left()));
}

void AuthPerformer::StartAuthSession(std::unique_ptr<UserContext> context,
                                     bool ephemeral,
                                     AuthSessionIntent intent,
                                     StartSessionCallback callback) {
  client_->WaitForServiceToBeAvailable(base::BindOnce(
      &AuthPerformer::OnServiceRunning, weak_factory_.GetWeakPtr(),
      std::move(context), ephemeral, intent, std::move(callback)));
}

void AuthPerformer::OnServiceRunning(std::unique_ptr<UserContext> context,
                                     bool ephemeral,
                                     AuthSessionIntent intent,
                                     StartSessionCallback callback,
                                     bool service_is_available) {
  if (!service_is_available) {
    // TODO(crbug.com/1262139): Maybe have this error surfaced to UI.
    LOG(FATAL) << "Cryptohome service could not start";
  }
  LOGIN_LOG(EVENT) << "Starting AuthSession";
  user_data_auth::StartAuthSessionRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(context->GetAccountId());
  request.set_intent(SerializeIntent(intent));

  if (ephemeral) {
    request.set_flags(user_data_auth::AUTH_SESSION_FLAGS_EPHEMERAL_USER);
  } else {
    request.set_flags(user_data_auth::AUTH_SESSION_FLAGS_NONE);
  }

  client_->StartAuthSession(
      request, base::BindOnce(&AuthPerformer::OnStartAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::InvalidateAuthSession(std::unique_ptr<UserContext> context,
                                          AuthOperationCallback callback) {
  CHECK(!context->GetAuthSessionId().empty());

  user_data_auth::InvalidateAuthSessionRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  client_->InvalidateAuthSession(
      request, base::BindOnce(&AuthPerformer::OnInvalidateAuthSession,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::PrepareAuthFactor(std::unique_ptr<UserContext> context,
                                      cryptohome::AuthFactorType type,
                                      AuthOperationCallback callback) {
  CHECK(!context->GetAuthSessionId().empty());

  user_data_auth::PrepareAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_auth_factor_type(cryptohome::ConvertFactorTypeToProto(type));
  request.set_purpose(::user_data_auth::PURPOSE_AUTHENTICATE_AUTH_FACTOR);

  client_->PrepareAuthFactor(
      request, base::BindOnce(&AuthPerformer::OnPrepareAuthFactor,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::TerminateAuthFactor(std::unique_ptr<UserContext> context,
                                        cryptohome::AuthFactorType type,
                                        AuthOperationCallback callback) {
  CHECK(!context->GetAuthSessionId().empty());
  user_data_auth::TerminateAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_auth_factor_type(cryptohome::ConvertFactorTypeToProto(type));

  client_->TerminateAuthFactor(
      request, base::BindOnce(&AuthPerformer::OnTerminateAuthFactor,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthPerformer::AuthenticateUsingKnowledgeKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(context->GetChallengeResponseKeys().empty());
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";

  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    DCHECK(!context->IsUsingPin());
    SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
        &AuthPerformer::HashKeyAndAuthenticate, weak_factory_.GetWeakPtr(),
        std::move(context), std::move(callback)));
    return;
  }  // plain-text password

  auto* key = context->GetKey();
  const auto& auth_factors = context->GetAuthFactorsData();

  // The login code might speculatively set the "gaia" label in the user
  // context, however at the cryptohome level the existing user key's label can
  // be either "gaia", "local-password" or "legacy-N" - which is what we need to
  // use when talking to cryptohome. If in cryptohome, "gaia" is indeed the
  // label, then at the end of this operation, gaia would be returned. This case
  // applies to only "gaia" labels only because they are created at oobe.
  if (key->GetLabel() == kCryptohomeGaiaKeyLabel || key->GetLabel().empty()) {
    const auto* factor = auth_factors.FindAnyPasswordFactor();
    if (factor == nullptr) {
      LOGIN_LOG(ERROR) << "Could not find Password key";
      std::move(callback).Run(
          std::move(context),
          AuthenticationError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
      return;
    }
    key->SetLabel(factor->ref().label().value());
  }

  LOGIN_LOG(EVENT) << "Authenticating using factor "
                   << context->GetKey()->GetKeyType();
  user_data_auth::AuthenticateAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  if (context->IsUsingPin()) {
    cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPin,
                                  cryptohome::KeyLabel{key->GetLabel()}};
    cryptohome::AuthFactorInput input(
        cryptohome::AuthFactorInput::Pin{key->GetSecret()});
    cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
    request.set_auth_factor_label(ref.label().value());
  } else {
    cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPassword,
                                  cryptohome::KeyLabel{key->GetLabel()}};
    cryptohome::AuthFactorInput input(
        cryptohome::AuthFactorInput::Password{key->GetSecret()});

    cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
    request.set_auth_factor_label(ref.label().value());
  }
  client_->AuthenticateAuthFactor(
      request,
      base::BindOnce(&AuthPerformer::MaybeRecordKnowledgeFactorAuthFailure,
                     weak_factory_.GetWeakPtr(), clock_->Now(),
                     std::move(context), std::move(callback)));
}

void AuthPerformer::MaybeRecordKnowledgeFactorAuthFailure(
    base::Time request_start,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::AuthenticateAuthFactorReply> reply) {
  if (auto error = user_data_auth::ReplyToCryptohomeError(reply);
      cryptohome::ErrorMatches(
          error, user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND)) {
    AuthEventsRecorder::Get()->OnKnowledgeFactorAuthFailure();
  }
  OnAuthenticateAuthFactor(request_start, std::move(context),
                           std::move(callback), std::move(reply));
}

void AuthPerformer::HashKeyAndAuthenticate(std::unique_ptr<UserContext> context,
                                           AuthOperationCallback callback,
                                           const std::string& system_salt) {
  context->GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                               system_salt);
  AuthenticateUsingKnowledgeKey(std::move(context), std::move(callback));
}

void AuthPerformer::AuthenticateUsingChallengeResponseKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!context->GetChallengeResponseKeys().empty());
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";
  LOGIN_LOG(EVENT) << "Authenticating using challenge-response";

  user_data_auth::AuthenticateAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  DCHECK_EQ(context->GetChallengeResponseKeys().size(), 1ull);

  auto label =
      GenerateChallengeResponseKeyLabel(context->GetChallengeResponseKeys());
  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kSmartCard,
                                cryptohome::KeyLabel{label}};
  cryptohome::AuthFactorInput input(cryptohome::AuthFactorInput::SmartCard{
      context->GetChallengeResponseKeys()[0].signature_algorithms(),
      cryptohome::kCryptohomeKeyDelegateServiceName,
  });
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
  request.set_auth_factor_label(ref.label().value());
  client_->AuthenticateAuthFactor(
      request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthFactor,
                              weak_factory_.GetWeakPtr(), clock_->Now(),
                              std::move(context), std::move(callback)));
}

void AuthPerformer::AuthenticateWithPassword(
    const std::string& key_label,
    const std::string& password,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!password.empty()) << "Caller should check for empty password";
  DCHECK(!key_label.empty()) << "Caller should provide correct label";
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";

  const SessionAuthFactors& auth_factors = context->GetAuthFactorsData();
  if (auth_factors.FindPasswordFactor(cryptohome::KeyLabel{key_label}) ==
      nullptr) {
    LOGIN_LOG(ERROR) << "User does not have password factor labeled "
                     << key_label;
    std::move(callback).Run(
        std::move(context),
        AuthenticationError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
    return;
  }
  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &AuthPerformer::HashPasswordAndAuthenticate, weak_factory_.GetWeakPtr(),
      key_label, password, std::move(context), std::move(callback)));
}

void AuthPerformer::HashPasswordAndAuthenticate(
    const std::string& key_label,
    const std::string& password,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    const std::string& system_salt) {
  // Use Key until proper migration to AuthFactors API.
  Key password_key(password);
  password_key.SetLabel(key_label);
  password_key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);
  context->SetKey(password_key);
  AuthenticateUsingKnowledgeKey(std::move(context), std::move(callback));
}

void AuthPerformer::AuthenticateWithPin(const std::string& pin,
                                        const std::string& pin_salt,
                                        std::unique_ptr<UserContext> context,
                                        AuthOperationCallback callback) {
  DCHECK(!pin.empty()) << "Caller should check for empty PIN";
  DCHECK(!pin_salt.empty()) << "Client code should provide correct salt";
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";

  // Use Key until proper migration to AuthFactors API.
  Key key(pin);

  const auto& auth_factors = context->GetAuthFactorsData();
  auto* factor = auth_factors.FindPinFactor();
  if (factor == nullptr) {
    LOGIN_LOG(ERROR) << "User does not have PIN as factor";
    std::move(callback).Run(
        std::move(context),
        AuthenticationError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
    return;
  }
  DCHECK_EQ(factor->ref().label().value(), kCryptohomePinLabel);
  key.SetLabel(factor->ref().label().value());

  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, pin_salt);
  context->SetKey(std::move(key));
  context->SetIsUsingPin(true);
  AuthenticateUsingKnowledgeKey(std::move(context), std::move(callback));
}

void AuthPerformer::AuthenticateAsKiosk(std::unique_ptr<UserContext> context,
                                        AuthOperationCallback callback) {
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";

  LOGIN_LOG(EVENT) << "Authenticating as Kiosk";

  const auto& auth_factors = context->GetAuthFactorsData();

  user_data_auth::AuthenticateAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());
  auto* existing_factor = auth_factors.FindKioskFactor();
  if (existing_factor == nullptr) {
    LOGIN_LOG(ERROR) << "Could not find Kiosk key";
    std::move(callback).Run(
        std::move(context),
        AuthenticationError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
    return;
  }
  cryptohome::AuthFactorInput input(cryptohome::AuthFactorInput::Kiosk{});
  cryptohome::SerializeAuthInput(existing_factor->ref(), input,
                                 request.mutable_auth_input());
  request.set_auth_factor_label(existing_factor->ref().label().value());
  client_->AuthenticateAuthFactor(
      request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthFactor,
                              weak_factory_.GetWeakPtr(), clock_->Now(),
                              std::move(context), std::move(callback)));
}

void AuthPerformer::GetAuthSessionStatus(std::unique_ptr<UserContext> context,
                                         AuthSessionStatusCallback callback) {
  if (context->GetAuthSessionId().empty())
    NOTREACHED() << "Auth session should exist";

  LOGIN_LOG(EVENT) << "Requesting authsession status";
  user_data_auth::GetAuthSessionStatusRequest request;

  request.set_auth_session_id(context->GetAuthSessionId());

  client_->GetAuthSessionStatus(
      request, base::BindOnce(&AuthPerformer::OnGetAuthSessionStatus,
                              weak_factory_.GetWeakPtr(), clock_->Now(),
                              std::move(context), std::move(callback)));
}

void AuthPerformer::ExtendAuthSessionLifetime(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  if (context->GetAuthSessionId().empty()) {
    NOTREACHED() << "Auth session should exist";
    return;
  }
  LOGIN_LOG(EVENT) << "Requesting authsession lifetime extension";
  user_data_auth::ExtendAuthSessionRequest request;

  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_extension_duration(
      cryptohome::kAuthsessionExtensionPeriod.InSeconds());

  client_->ExtendAuthSession(
      request, base::BindOnce(&AuthPerformer::OnExtendAuthSession,
                              weak_factory_.GetWeakPtr(), clock_->Now(),
                              std::move(context), std::move(callback)));
}

void AuthPerformer::GetRecoveryRequest(
    const std::string& access_token,
    const CryptohomeRecoveryEpochResponse& epoch,
    std::unique_ptr<UserContext> context,
    RecoveryRequestCallback callback) {
  if (context->GetAuthSessionId().empty()) {
    NOTREACHED() << "Auth session should exist";
  }

  LOGIN_LOG(EVENT) << "Obtaining RecoveryRequest";

  user_data_auth::GetRecoveryRequestRequest request;

  request.set_auth_session_id(context->GetAuthSessionId());

  const std::string& gaia_id = context->GetGaiaID();
  CHECK(!gaia_id.empty()) << "Recovery is only supported for gaia users";
  CHECK(!access_token.empty());
  const std::string& reauth_proof_token = context->GetReauthProofToken();
  CHECK(!reauth_proof_token.empty()) << "Reauth proof token must be set";

  request.set_requestor_user_id_type(
      user_data_auth::GetRecoveryRequestRequest::GAIA_ID);
  request.set_requestor_user_id(gaia_id);
  request.set_auth_factor_label(kCryptohomeRecoveryKeyLabel);
  request.set_gaia_access_token(access_token);
  request.set_gaia_reauth_proof_token(reauth_proof_token);
  request.set_epoch_response(epoch->data(), epoch->size());

  client_->GetRecoveryRequest(
      std::move(request),
      base::BindOnce(&AuthPerformer::OnGetRecoveryRequest,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(context)));
}

void AuthPerformer::AuthenticateWithRecovery(
    const CryptohomeRecoveryEpochResponse& epoch,
    const CryptohomeRecoveryResponse& recovery_response,
    const RecoveryLedgerName ledger_name,
    const RecoveryLedgerPubKey ledger_public_key,
    uint32_t ledger_public_key_hash,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  if (context->GetAuthSessionId().empty()) {
    NOTREACHED() << "Auth session should exist";
  }

  LOGIN_LOG(EVENT) << "Authenticating via Recovery";

  user_data_auth::AuthenticateAuthFactorRequest request;

  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_auth_factor_label(kCryptohomeRecoveryKeyLabel);

  user_data_auth::CryptohomeRecoveryAuthInput* recovery_input =
      request.mutable_auth_input()->mutable_cryptohome_recovery_input();
  recovery_input->set_epoch_response(epoch->data(), epoch->size());
  recovery_input->set_recovery_response(recovery_response->data(),
                                        recovery_response->size());
  user_data_auth::CryptohomeRecoveryAuthInput::LedgerInfo* ledger =
      recovery_input->mutable_ledger_info();
  ledger->set_name(ledger_name.value());
  ledger->set_key_hash(ledger_public_key_hash);
  ledger->set_public_key(ledger_public_key.value());

  client_->AuthenticateAuthFactor(
      request, base::BindOnce(&AuthPerformer::OnAuthenticateAuthFactor,
                              weak_factory_.GetWeakPtr(), clock_->Now(),
                              std::move(context), std::move(callback)));
}

/// ---- private callbacks ----

void AuthPerformer::OnStartAuthSession(
    std::unique_ptr<UserContext> context,
    StartSessionCallback callback,
    absl::optional<user_data_auth::StartAuthSessionReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "Could not start authsession " << error;
    std::move(callback).Run(false, std::move(context),
                            AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "AuthSession started, user "
                   << (reply->user_exists() ? "exists" : "does not exist");

  context->SetAuthSessionIds(reply->auth_session_id(), reply->broadcast_id());

  std::vector<cryptohome::AuthFactor> next_factors;
  cryptohome::AuthFactorType fallback_type =
      cryptohome::AuthFactorType::kPassword;
  if (IsKioskUserType(context->GetUserType())) {
    fallback_type = cryptohome::AuthFactorType::kKiosk;
  }

  // Ignore unknown factors that are in development.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool ignore_unknown_factors =
      command_line->HasSwitch(ash::switches::kIgnoreUnknownAuthFactors);

  for (const auto& factor_proto : reply->auth_factors()) {
    if (ignore_unknown_factors &&
        !cryptohome::SafeConvertFactorTypeFromProto(factor_proto.type())) {
      continue;
    }
    next_factors.emplace_back(
        cryptohome::DeserializeAuthFactor(factor_proto, fallback_type));
  }

  SessionAuthFactors auth_factors_data(std::move(next_factors));
  context->SetSessionAuthFactors(std::move(auth_factors_data));

  std::move(callback).Run(reply->user_exists(), std::move(context),
                          absl::nullopt);
}

void AuthPerformer::OnInvalidateAuthSession(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::InvalidateAuthSessionReply> reply) {
  // The auth session is useless even if we failed to invalidate it.
  context->ResetAuthSessionIds();

  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error) &&
      !cryptohome::ErrorMatches(
          error, user_data_auth::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN)) {
    LOGIN_LOG(ERROR) << "Could not invalidate authsession " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }

  std::move(callback).Run(std::move(context), absl::nullopt);
}

void AuthPerformer::OnPrepareAuthFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::PrepareAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "Could not prepare auth factor " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }

  std::move(callback).Run(std::move(context), absl::nullopt);
}

void AuthPerformer::OnTerminateAuthFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::TerminateAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "Could not terminate auth factor " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }

  std::move(callback).Run(std::move(context), absl::nullopt);
}

void AuthPerformer::OnAuthenticateAuthFactor(
    base::Time request_start,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::AuthenticateAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(EVENT)
        << "Failed to authenticate session via authfactor, error code "
        << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  CHECK(reply->has_auth_properties());
  FillAuthenticationData(request_start, reply->auth_properties(), *context);

  LOGIN_LOG(EVENT) << "Authenticated successfully";
  std::move(callback).Run(std::move(context), absl::nullopt);
}

void AuthPerformer::OnGetAuthSessionStatus(
    base::Time request_start,
    std::unique_ptr<UserContext> context,
    AuthSessionStatusCallback callback,
    absl::optional<user_data_auth::GetAuthSessionStatusReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);

  if (cryptohome::ErrorMatches(
          error, user_data_auth::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN)) {
    // Do not trigger error handling
    std::move(callback).Run(AuthSessionStatus(), base::TimeDelta(),
                            std::move(context),
                            /*cryptohome_error=*/absl::nullopt);
    return;
  }

  if (cryptohome::HasError(error)) {
    LOGIN_LOG(EVENT) << "Failed to get authsession status " << error;
    std::move(callback).Run(AuthSessionStatus(), base::TimeDelta(),
                            std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  CHECK(reply->has_auth_properties());
  // TODO(b/301078137): As lifetime is now stored in UserContext,
  // there is no need to pass it separately.
  base::TimeDelta lifetime;
  AuthSessionStatus status;
  switch (reply->status()) {
    case ::user_data_auth::AUTH_SESSION_STATUS_NOT_SET:
    case ::user_data_auth::AUTH_SESSION_STATUS_INVALID_AUTH_SESSION:
      break;
    case ::user_data_auth::AUTH_SESSION_STATUS_FURTHER_FACTOR_REQUIRED:
      status.Put(AuthSessionLevel::kSessionIsValid);
      // Once we support multi-factor authentication (and have partially
      // authenticated sessions) we might need to use value from reply.
      lifetime = base::TimeDelta::Max();
      break;
    case ::user_data_auth::AUTH_SESSION_STATUS_AUTHENTICATED:
      status.Put(AuthSessionLevel::kSessionIsValid);
      status.Put(AuthSessionLevel::kCryptohomeStrong);
      lifetime = base::Seconds(reply->auth_properties().seconds_left());
      break;
    default:
      NOTREACHED();
  }
  FillAuthenticationData(request_start, reply->auth_properties(), *context);
  std::move(callback).Run(status, lifetime, std::move(context),
                          /*cryptohome_error=*/absl::nullopt);
}

void AuthPerformer::OnExtendAuthSession(
    base::Time request_start,
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    absl::optional<user_data_auth::ExtendAuthSessionReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(EVENT) << "Failed to extend authsession lifetime " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  context->SetSessionLifetime(request_start +
                              base::Seconds(reply->seconds_left()));
  std::move(callback).Run(std::move(context),
                          /*cryptohome_error=*/absl::nullopt);
}

void AuthPerformer::OnGetRecoveryRequest(
    RecoveryRequestCallback callback,
    std::unique_ptr<UserContext> context,
    absl::optional<user_data_auth::GetRecoveryRequestReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);

  if (cryptohome::HasError(error)) {
    LOGIN_LOG(EVENT) << "Failed to obtain recovery request, error code "
                     << error;
    std::move(callback).Run(absl::nullopt, std::move(context),
                            AuthenticationError{error});
    return;
  }

  CHECK(!reply->recovery_request().empty());
  std::move(callback).Run(RecoveryRequest(reply->recovery_request()),
                          std::move(context), absl::nullopt);
}

}  // namespace ash
