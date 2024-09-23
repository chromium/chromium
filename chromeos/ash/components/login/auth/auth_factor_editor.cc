// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_factor_editor.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/auth_factor_conversions.h"
#include "chromeos/ash/components/cryptohome/auth_factor_input.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/cryptohome/error_types.h"
#include "chromeos/ash/components/cryptohome/error_util.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/constants/cryptohome_key_delegate_constants.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/challenge_response/key_label_utils.h"
#include "chromeos/ash/components/login/auth/cryptohome_parameter_utils.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/recovery/service_constants.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user.h"

namespace ash {

using ::cryptohome::KeyLabel;

AuthFactorEditor::AuthFactorEditor(UserDataAuthClient* client)
    : client_(client) {
  CHECK(client_);
}

AuthFactorEditor::~AuthFactorEditor() = default;

void AuthFactorEditor::InvalidateCurrentAttempts() {
  weak_factory_.InvalidateWeakPtrs();
}

base::WeakPtr<AuthFactorEditor> AuthFactorEditor::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AuthFactorEditor::GetAuthFactorsConfiguration(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  LOGIN_LOG(EVENT) << "Listing AuthFactors";
  user_data_auth::ListAuthFactorsRequest request;

  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(context->GetAccountId());

  client_->ListAuthFactors(
      request, base::BindOnce(&AuthFactorEditor::OnListAuthFactors,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::AddKioskKey(std::unique_ptr<UserContext> context,
                                   AuthOperationCallback callback) {
  const SessionAuthFactors& auth_factors = context->GetAuthFactorsData();
  auto* existing_factor = auth_factors.FindKioskFactor();
  if (existing_factor != nullptr) {
    LOGIN_LOG(ERROR) << "Adding Kiosk key while one already exists";
    std::move(callback).Run(
        std::move(context),
        AuthenticationError{cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
            user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED)});
    return;
  }

  LOGIN_LOG(EVENT) << "Adding Kiosk key";

  user_data_auth::AddAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kKiosk,
                                KeyLabel{kCryptohomePublicMountLabel}};
  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::AuthFactor factor(ref, std::move(metadata));

  cryptohome::AuthFactorInput input(cryptohome::AuthFactorInput::Kiosk{});
  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
  client_->AddAuthFactor(
      request, base::BindOnce(&AuthFactorEditor::OnAddAuthFactor,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::AddContextKnowledgeKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    DCHECK(!context->IsUsingPin());
    if (context->GetKey()->GetLabel().empty()) {
      context->GetKey()->SetLabel(kCryptohomeGaiaKeyLabel);
    }  // empty label
    SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
        &AuthFactorEditor::HashContextKeyAndAdd, weak_factory_.GetWeakPtr(),
        std::move(context), std::move(callback)));
    return;
  }  // plain-text password

  LOGIN_LOG(EVENT) << "Adding knowledge key from the context "
                   << context->GetKey()->GetKeyType();

  auto* key = context->GetKey();
  DCHECK(!key->GetLabel().empty());

  user_data_auth::AddAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  // For now convert Key structure.
  if (context->IsUsingPin()) {
    cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPin,
                                  KeyLabel{key->GetLabel()}};
    cryptohome::AuthFactorCommonMetadata metadata;
    cryptohome::PinMetadata pin_metadata =
        cryptohome::PinMetadata::CreateWithoutSalt();
    cryptohome::AuthFactor factor(ref, std::move(metadata),
                                  std::move(pin_metadata));

    cryptohome::AuthFactorInput input(
        cryptohome::AuthFactorInput::Pin{key->GetSecret()});
    cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
    cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
  } else {
    cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPassword,
                                  KeyLabel{key->GetLabel()}};
    cryptohome::AuthFactorCommonMetadata metadata;
    cryptohome::PasswordMetadata password_metadata =
        cryptohome::PasswordMetadata::CreateWithoutSalt();
    cryptohome::AuthFactor factor(ref, std::move(metadata),
                                  std::move(password_metadata));

    cryptohome::AuthFactorInput input(
        cryptohome::AuthFactorInput::Password{key->GetSecret()});
    cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
    cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
  }
  client_->AddAuthFactor(
      request, base::BindOnce(&AuthFactorEditor::OnAddAuthFactor,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::AddContextChallengeResponseKey(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!context->GetChallengeResponseKeys().empty());

  LOGIN_LOG(EVENT) << "Adding challenge-response key from the context";
  user_data_auth::AddAuthFactorRequest request;
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

  cryptohome::SmartCardMetadata smart_card_metadata{
      context->GetChallengeResponseKeys()[0].public_key_spki_der()};

  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::AuthFactor factor(ref, std::move(metadata),
                                std::move(smart_card_metadata));

  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

  client_->AddAuthFactor(
      request, base::BindOnce(&AuthFactorEditor::OnAddAuthFactor,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::HashContextKeyAndAdd(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    const std::string& system_salt) {
  context->GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                               system_salt);
  AddContextKnowledgeKey(std::move(context), std::move(callback));
}

void AuthFactorEditor::ReplaceContextKey(std::unique_ptr<UserContext> context,
                                         AuthOperationCallback callback) {
  DCHECK(!context->GetAuthSessionId().empty());
  DCHECK(context->HasReplacementKey());
  DCHECK(!context->IsUsingPin());
  DCHECK(!context->GetKey()->GetLabel().empty());

  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN ||
      context->GetReplacementKey()->GetKeyType() ==
          Key::KEY_TYPE_PASSWORD_PLAIN) {
    SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
        &AuthFactorEditor::HashContextKeyAndReplace, weak_factory_.GetWeakPtr(),
        std::move(context), std::move(callback)));
    return;
  }

  LOGIN_LOG(EVENT) << "Replacing key from context "
                   << context->GetReplacementKey()->GetKeyType();

  user_data_auth::UpdateAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  auto* old_key = context->GetKey();
  auto* key = context->GetReplacementKey();

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPassword,
                                KeyLabel{old_key->GetLabel()}};

  request.set_auth_factor_label(ref.label().value());

  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::PasswordMetadata password_metadata =
      cryptohome::PasswordMetadata::CreateWithoutSalt();
  cryptohome::AuthFactor factor(ref, std::move(metadata),
                                std::move(password_metadata));

  cryptohome::AuthFactorInput input(
      cryptohome::AuthFactorInput::Password{key->GetSecret()});
  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
  client_->UpdateAuthFactor(
      request, base::BindOnce(&AuthFactorEditor::OnUpdateAuthFactor,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::HashContextKeyAndReplace(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    const std::string& system_salt) {
  if (context->GetKey()->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN) {
    context->GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                                 system_salt);
  }
  if (context->GetReplacementKey()->GetKeyType() ==
      Key::KEY_TYPE_PASSWORD_PLAIN) {
    context->GetReplacementKey()->Transform(
        Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);
  }
  ReplaceContextKey(std::move(context), std::move(callback));
}

void AuthFactorEditor::AddPinFactor(std::unique_ptr<UserContext> context,
                                    cryptohome::PinSalt salt,
                                    cryptohome::RawPin pin,
                                    AuthOperationCallback callback) {
  DCHECK(!context->GetAuthSessionId().empty());

  user_data_auth::AddAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPin,
                                KeyLabel{kCryptohomePinLabel}};

  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::PinMetadata pin_metadata =
      cryptohome::PinMetadata::Create(salt);
  cryptohome::AuthFactor factor(ref, std::move(metadata),
                                std::move(pin_metadata));

  Key key{std::move(*pin)};
  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, std::move(*salt));

  const cryptohome::AuthFactorInput input(
      cryptohome::AuthFactorInput::Pin{key.GetSecret()});
  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

  auto on_added_callback = base::BindOnce(
      &AuthFactorEditor::OnAddAuthFactor, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));
  LOGIN_LOG(EVENT) << "Adding pin factor";
  client_->AddAuthFactor(std::move(request), std::move(on_added_callback));
}

void AuthFactorEditor::ReplacePinFactor(std::unique_ptr<UserContext> context,
                                        cryptohome::PinSalt salt,
                                        cryptohome::RawPin pin,
                                        AuthOperationCallback callback) {
  DCHECK(!context->GetAuthSessionId().empty());

  user_data_auth::UpdateAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  request.set_auth_factor_label(kCryptohomePinLabel);

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPin,
                                KeyLabel{kCryptohomePinLabel}};

  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::PinMetadata pin_metadata =
      cryptohome::PinMetadata::Create(salt);
  cryptohome::AuthFactor factor(ref, std::move(metadata),
                                std::move(pin_metadata));

  Key key{std::move(*pin)};
  key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, std::move(*salt));

  const cryptohome::AuthFactorInput input(
      cryptohome::AuthFactorInput::Pin{key.GetSecret()});
  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

  auto on_updated_callback = base::BindOnce(
      &AuthFactorEditor::OnUpdateAuthFactor, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));
  LOGIN_LOG(EVENT) << "Replacing pin factor";
  client_->UpdateAuthFactor(std::move(request), std::move(on_updated_callback));
}

void AuthFactorEditor::RemovePinFactor(std::unique_ptr<UserContext> context,
                                       AuthOperationCallback callback) {
  DCHECK(!context->GetAuthSessionId().empty());

  LOGIN_LOG(EVENT) << "Removing pin factor";

  user_data_auth::RemoveAuthFactorRequest req;
  req.set_auth_session_id(context->GetAuthSessionId());
  req.set_auth_factor_label(kCryptohomePinLabel);

  auto remove_auth_factor_callback = base::BindOnce(
      &AuthFactorEditor::OnRemoveAuthFactor, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));
  client_->RemoveAuthFactor(req, std::move(remove_auth_factor_callback));
}

void AuthFactorEditor::AddRecoveryFactor(std::unique_ptr<UserContext> context,
                                         AuthOperationCallback callback) {
  DCHECK(!context->GetAuthSessionId().empty());

  // TODO(crbug.com/40219817): Check whether a recovery key already exists and
  // return immediately.

  LOGIN_LOG(EVENT) << "Adding recovery key";

  user_data_auth::AddAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kRecovery,
                                KeyLabel{kCryptohomeRecoveryKeyLabel}};
  cryptohome::CryptohomeRecoveryMetadata recovery_metadata{
      GetRecoveryHsmPublicKey()};
  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::AuthFactor factor(ref, std::move(metadata),
                                std::move(recovery_metadata));

  cryptohome::AuthFactorInput input(
      cryptohome::AuthFactorInput::RecoveryCreation{
          GetRecoveryHsmPublicKey(), context->GetGaiaID(),
          context->GetDeviceId(),
          /*ensure_fresh_recovery_id=*/true});

  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

  auto add_auth_factor_callback = base::BindOnce(
      &AuthFactorEditor::OnAddAuthFactor, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));

  client_->AddAuthFactor(std::move(request),
                         std::move(add_auth_factor_callback));
}

void AuthFactorEditor::RotateRecoveryFactor(
    std::unique_ptr<UserContext> context,
    bool ensure_fresh_recovery_id,
    AuthOperationCallback callback) {
  CHECK(!context->GetAuthSessionId().empty());

  LOGIN_LOG(EVENT) << "Rotating recovery key";

  user_data_auth::UpdateAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());
  request.set_auth_factor_label(kCryptohomeRecoveryKeyLabel);

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kRecovery,
                                KeyLabel{kCryptohomeRecoveryKeyLabel}};
  cryptohome::CryptohomeRecoveryMetadata recovery_metadata{
      GetRecoveryHsmPublicKey()};
  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::AuthFactor factor(ref, std::move(metadata),
                                std::move(recovery_metadata));

  cryptohome::AuthFactorInput input(
      cryptohome::AuthFactorInput::RecoveryCreation{
          GetRecoveryHsmPublicKey(), context->GetGaiaID(),
          context->GetDeviceId(), ensure_fresh_recovery_id});

  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());

  auto on_updated_callback = base::BindOnce(
      &AuthFactorEditor::OnUpdateAuthFactor, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));
  client_->UpdateAuthFactor(std::move(request), std::move(on_updated_callback));
}

void AuthFactorEditor::RemoveRecoveryFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback) {
  DCHECK(!context->GetAuthSessionId().empty());

  // TODO(crbug.com/40219817): Check whether a recovery key already exists and
  // return immediately if there are no recovery keys.

  LOGIN_LOG(EVENT) << "Removing recovery key";

  user_data_auth::RemoveAuthFactorRequest req;
  req.set_auth_session_id(context->GetAuthSessionId());
  req.set_auth_factor_label(kCryptohomeRecoveryKeyLabel);

  auto remove_auth_factor_callback = base::BindOnce(
      &AuthFactorEditor::OnRemoveAuthFactor, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));
  client_->RemoveAuthFactor(req, std::move(remove_auth_factor_callback));
}

void AuthFactorEditor::SetPasswordFactor(std::unique_ptr<UserContext> context,
                                         cryptohome::RawPassword new_password,
                                         const cryptohome::KeyLabel& label,
                                         AuthOperationCallback callback) {
  LOGIN_LOG(EVENT) << "Setting password with label: " << label;

  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &AuthFactorEditor::SetPasswordFactorImpl, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(new_password), std::move(label),
      std::move(callback)));
}

void AuthFactorEditor::UpdatePasswordFactor(
    std::unique_ptr<UserContext> context,
    cryptohome::RawPassword new_password,
    const cryptohome::KeyLabel& label,
    AuthOperationCallback callback) {
  LOGIN_LOG(EVENT) << "Updating password with label: " << label;

  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &AuthFactorEditor::UpdatePasswordFactorImpl, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(new_password), std::move(label),
      std::move(callback)));
}

void AuthFactorEditor::ReplacePasswordFactor(
    std::unique_ptr<UserContext> context,
    const cryptohome::KeyLabel& old_label,
    cryptohome::RawPassword new_password,
    const cryptohome::KeyLabel& new_label,
    AuthOperationCallback callback) {
  LOGIN_LOG(EVENT) << "Replacing password with label " << old_label << " to "
                   << new_label;

  SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &AuthFactorEditor::ReplacePasswordFactorImpl, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(old_label), std::move(new_password),
      std::move(new_label), std::move(callback)));
}

void AuthFactorEditor::SetPasswordFactorImpl(
    std::unique_ptr<UserContext> context,
    cryptohome::RawPassword new_password,
    const cryptohome::KeyLabel& label,
    AuthOperationCallback callback,
    const std::string& system_salt) {
  Key key{std::move(new_password).value()};
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);

  user_data_auth::AddAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPassword, label};

  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::PasswordMetadata password_metadata =
      label == cryptohome::KeyLabel{kCryptohomeLocalPasswordKeyLabel}
          ? cryptohome::PasswordMetadata::CreateForLocalPassword(
                cryptohome::SystemSalt(system_salt))
          : cryptohome::PasswordMetadata::CreateForOnlinePassword(
                cryptohome::SystemSalt(system_salt));
  cryptohome::AuthFactor factor(ref, std::move(metadata),
                                std::move(password_metadata));

  cryptohome::AuthFactorInput input(
      cryptohome::AuthFactorInput::Password{std::move(key.GetSecret())});
  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
  client_->AddAuthFactor(
      request, base::BindOnce(&AuthFactorEditor::OnAddAuthFactor,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::UpdatePasswordFactorImpl(
    std::unique_ptr<UserContext> context,
    cryptohome::RawPassword new_password,
    const cryptohome::KeyLabel& label,
    AuthOperationCallback callback,
    const std::string& system_salt) {
  Key key{std::move(new_password).value()};
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);

  user_data_auth::UpdateAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPassword, label};

  request.set_auth_factor_label(ref.label().value());

  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::PasswordMetadata password_metadata =
      label == cryptohome::KeyLabel{kCryptohomeLocalPasswordKeyLabel}
          ? cryptohome::PasswordMetadata::CreateForLocalPassword(
                cryptohome::SystemSalt(system_salt))
          : cryptohome::PasswordMetadata::CreateForOnlinePassword(
                cryptohome::SystemSalt(system_salt));
  cryptohome::AuthFactor factor(ref, std::move(metadata),
                                std::move(password_metadata));

  cryptohome::AuthFactorInput input(
      cryptohome::AuthFactorInput::Password{std::move(key.GetSecret())});
  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(ref, input, request.mutable_auth_input());
  client_->UpdateAuthFactor(
      request, base::BindOnce(&AuthFactorEditor::OnUpdateAuthFactor,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::ReplacePasswordFactorImpl(
    std::unique_ptr<UserContext> context,
    const cryptohome::KeyLabel& old_label,
    cryptohome::RawPassword new_password,
    const cryptohome::KeyLabel& new_label,
    AuthOperationCallback callback,
    const std::string& system_salt) {
  CHECK(new_label != old_label);

  Key key{std::move(new_password).value()};
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);

  user_data_auth::ReplaceAuthFactorRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::AuthFactorRef old_ref{cryptohome::AuthFactorType::kPassword,
                                    old_label};

  request.set_auth_factor_label(old_ref.label().value());

  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::PasswordMetadata password_metadata =
      new_label == cryptohome::KeyLabel{kCryptohomeLocalPasswordKeyLabel}
          ? cryptohome::PasswordMetadata::CreateForLocalPassword(
                cryptohome::SystemSalt(system_salt))
          : cryptohome::PasswordMetadata::CreateForOnlinePassword(
                cryptohome::SystemSalt(system_salt));
  cryptohome::AuthFactorRef new_ref{cryptohome::AuthFactorType::kPassword,
                                    new_label};
  cryptohome::AuthFactor factor(new_ref, std::move(metadata),
                                std::move(password_metadata));

  cryptohome::AuthFactorInput input(
      cryptohome::AuthFactorInput::Password{std::move(key.GetSecret())});
  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  cryptohome::SerializeAuthInput(new_ref, input, request.mutable_auth_input());
  client_->ReplaceAuthFactor(
      request, base::BindOnce(&AuthFactorEditor::OnReplaceAuthFactor,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::UpdatePasswordFactorMetadata(
    std::unique_ptr<UserContext> context,
    const cryptohome::KeyLabel& label,
    const cryptohome::SystemSalt& system_salt,
    AuthOperationCallback callback) {
  LOGIN_LOG(EVENT) << "Updating password metadata with label: " << label;

  user_data_auth::UpdateAuthFactorMetadataRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPassword, label};

  request.set_auth_factor_label(ref.label().value());

  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::PasswordMetadata password_metadata =
      label == cryptohome::KeyLabel{kCryptohomeLocalPasswordKeyLabel}
          ? cryptohome::PasswordMetadata::CreateForLocalPassword(system_salt)
          : cryptohome::PasswordMetadata::CreateForOnlinePassword(system_salt);
  cryptohome::AuthFactor factor(ref, std::move(metadata),
                                std::move(password_metadata));

  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());
  client_->UpdateAuthFactorMetadata(
      request, base::BindOnce(&AuthFactorEditor::OnUpdateAuthFactorMetadata,
                              weak_factory_.GetWeakPtr(), std::move(context),
                              std::move(callback)));
}

void AuthFactorEditor::UpdatePinFactorMetadata(
    std::unique_ptr<UserContext> context,
    cryptohome::PinSalt salt,
    AuthOperationCallback callback) {
  DCHECK(!context->GetAuthSessionId().empty());

  user_data_auth::UpdateAuthFactorMetadataRequest request;
  request.set_auth_session_id(context->GetAuthSessionId());

  request.set_auth_factor_label(kCryptohomePinLabel);

  cryptohome::AuthFactorRef ref{cryptohome::AuthFactorType::kPin,
                                KeyLabel{kCryptohomePinLabel}};

  cryptohome::AuthFactorCommonMetadata metadata;
  cryptohome::PinMetadata pin_metadata = cryptohome::PinMetadata::Create(salt);
  cryptohome::AuthFactor factor(ref, std::move(metadata),
                                std::move(pin_metadata));

  cryptohome::SerializeAuthFactor(factor, request.mutable_auth_factor());

  auto on_updated_callback = base::BindOnce(
      &AuthFactorEditor::OnUpdateAuthFactorMetadata, weak_factory_.GetWeakPtr(),
      std::move(context), std::move(callback));
  LOGIN_LOG(EVENT) << "Updating pin factor metadata";
  client_->UpdateAuthFactorMetadata(std::move(request),
                                    std::move(on_updated_callback));
}

/// ---- private callbacks ----

void AuthFactorEditor::OnListAuthFactors(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::ListAuthFactorsReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "Could not list auth factors " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Got list of configured auth factors";

  std::vector<cryptohome::AuthFactor> factor_list;
  cryptohome::AuthFactorType fallback_type =
      cryptohome::AuthFactorType::kPassword;
  if (user_manager::User::TypeIsKiosk(context->GetUserType()))
    fallback_type = cryptohome::AuthFactorType::kKiosk;

  // Ignore unknown factors that are in development.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool ignore_unknown_factors =
      command_line->HasSwitch(ash::switches::kIgnoreUnknownAuthFactors);

  for (const auto& factor_with_status_proto :
       reply->configured_auth_factors_with_status()) {
    if (ignore_unknown_factors &&
        !cryptohome::SafeConvertFactorTypeFromProto(
            factor_with_status_proto.auth_factor().type())) {
      continue;
    }
    auto factor = cryptohome::DeserializeAuthFactor(factor_with_status_proto,
                                                    fallback_type);
    factor_list.emplace_back(std::move(factor));
  }
  cryptohome::AuthFactorsSet supported_factors;
  for (const auto proto_type : reply->supported_auth_factors()) {
    // TODO(crbug.com/40887032): This is temporary workaround on the client side
    // before issue is fixed on the cryptohome side.
    //  AUTH_FACTOR_TYPE_LEGACY_FINGERPRINT is not supported for editing anyhow.
    if (proto_type == user_data_auth::AUTH_FACTOR_TYPE_LEGACY_FINGERPRINT) {
      continue;
    }
    // TODO(b/272312302): Actually handle the AUTH_FACTORY_TYPE_FINGERPRINT
    // value.
    if (proto_type == user_data_auth::AUTH_FACTOR_TYPE_FINGERPRINT) {
      continue;
    }
    std::optional<cryptohome::AuthFactorType> type =
        cryptohome::SafeConvertFactorTypeFromProto(
            static_cast<user_data_auth::AuthFactorType>(proto_type));
    if (!type.has_value()) {
      LOG(ERROR) << " Unknown supported factor type, ignoring";
      base::debug::DumpWithoutCrashing(FROM_HERE);
      continue;
    }
    supported_factors.Put(type.value());
  }

  AuthFactorsConfiguration configured_factors(std::move(factor_list),
                                              supported_factors);
  context->SetAuthFactorsConfiguration(std::move(configured_factors));
  std::move(callback).Run(std::move(context), std::nullopt);
}

void AuthFactorEditor::OnAddAuthFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::AddAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "AddAuthFactor failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully added auth factor";
  context->ClearAuthFactorsConfiguration();
  std::move(callback).Run(std::move(context), std::nullopt);
  // TODO(crbug.com/40219817): Think if we should update SessionAuthFactors in
  // context after such operation.
}

void AuthFactorEditor::OnUpdateAuthFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::UpdateAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "UpdateAuthFactor failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully updated auth factor";
  context->ClearAuthFactorsConfiguration();
  std::move(callback).Run(std::move(context), std::nullopt);
}

void AuthFactorEditor::OnReplaceAuthFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::ReplaceAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "ReplaceAuthFactor failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully replaced auth factor";
  context->ClearAuthFactorsConfiguration();
  std::move(callback).Run(std::move(context), std::nullopt);
}

void AuthFactorEditor::OnRemoveAuthFactor(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<::user_data_auth::RemoveAuthFactorReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOG(WARNING) << "RemoveAuthFactor failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }

  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully removed auth factor";
  context->ClearAuthFactorsConfiguration();
  std::move(callback).Run(std::move(context), std::nullopt);
}

void AuthFactorEditor::OnUpdateAuthFactorMetadata(
    std::unique_ptr<UserContext> context,
    AuthOperationCallback callback,
    std::optional<user_data_auth::UpdateAuthFactorMetadataReply> reply) {
  auto error = user_data_auth::ReplyToCryptohomeError(reply);
  if (cryptohome::HasError(error)) {
    LOGIN_LOG(ERROR) << "UpdateAuthFactorMetadata failed with error " << error;
    std::move(callback).Run(std::move(context), AuthenticationError{error});
    return;
  }
  CHECK(reply.has_value());
  LOGIN_LOG(EVENT) << "Successfully updated auth factor metadata";
  context->ClearAuthFactorsConfiguration();
  std::move(callback).Run(std::move(context), std::nullopt);
}

}  // namespace ash
