// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/local_credential_management_win.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "device/fido/win/authenticator.h"
#include "device/fido/win/webauthn_api.h"

namespace {

bool ContainsUserCreatedCredential(
    const std::vector<device::DiscoverableCredentialMetadata>& credentials) {
  return base::ranges::any_of(
      credentials, [](const device::DiscoverableCredentialMetadata& cred) {
        return !cred.system_created;
      });
}

constexpr char kHasPlatformCredentialsPref[] =
    "webauthn.has_platform_credentials";

// CredentialPresenceCacher caches, in a `Profile` whether local credentials
// were found or not. This is done because we expect that enumerating platform
// credentials on Windows will get slower as the number of credentials
// increases, and we need to know whether there are any credentials in order to
// show the link (or not) on the passwords WebUI page.
//
// Thus, if credentials have been observed previously then that fact is cached
// and the link will appear on the passwords page without enumerating them
// again. Otherwise an enumeration will be attempted, which should be fast in
// the common case that there aren't any credentials.
//
// Since the platform authenticator is system-global, a `Profile` isn't quite
// the right sort of object to cache this information in. However, storing an
// installation-wide value would be much more work and, hopefully, this
// workaround can be eliminated in the future when webauthn.dll is faster.
//
// Since the `Profile` may be destroyed while the webauthn.dll call is still
// pending, this class observes the profile and handles that event.
class CredentialPresenceCacher : public ProfileObserver {
 public:
  CredentialPresenceCacher(
      Profile* profile,
      base::OnceCallback<void(
          std::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
          callback)
      : profile_(profile), callback_(std::move(callback)) {}

  ~CredentialPresenceCacher() override {
    if (profile_) {
      profile_->RemoveObserver(this);
      profile_ = nullptr;
    }
  }

  void OnEnumerateResult(
      std::vector<device::DiscoverableCredentialMetadata> credentials) {
    if (profile_) {
      profile_->GetPrefs()->SetBoolean(
          kHasPlatformCredentialsPref,
          ContainsUserCreatedCredential(credentials));
    }
    std::sort(credentials.begin(), credentials.end(), CredentialComparator());
    std::move(callback_).Run(std::move(credentials));
  }

  // ProfileObserver:

  void OnProfileWillBeDestroyed(Profile* profile) override {
    DCHECK_EQ(profile, profile_);
    profile_->RemoveObserver(this);
    profile_ = nullptr;
  }

 private:
  raw_ptr<Profile> profile_;
  base::OnceCallback<void(
      std::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
      callback_;
};

void EnumerateResultToBool(
    base::OnceCallback<void(bool)> callback,
    std::optional<std::vector<device::DiscoverableCredentialMetadata>>
        credentials) {
  std::move(callback).Run(credentials.has_value() &&
                          ContainsUserCreatedCredential(*credentials));
}

}  // namespace

LocalCredentialManagementWin::LocalCredentialManagementWin(
    device::WinWebAuthnApi* api,
    Profile* profile)
    : api_(api), profile_(profile) {}

// static
void LocalCredentialManagementWin::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kHasPlatformCredentialsPref, false);
}

std::unique_ptr<LocalCredentialManagement> LocalCredentialManagement::Create(
    Profile* profile) {
  return std::make_unique<LocalCredentialManagementWin>(
      device::WinWebAuthnApi::GetDefault(), profile);
}

void LocalCredentialManagementWin::HasCredentials(
    base::OnceCallback<void(bool)> callback) {
  std::optional<bool> result;

  if (!api_->IsAvailable() || !api_->SupportsSilentDiscovery()) {
    result = false;
  } else if (profile_->GetPrefs()->GetBoolean(kHasPlatformCredentialsPref)) {
    result = true;
  }

  if (result.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), *result));
    return;
  }

  auto cacher = std::make_unique<CredentialPresenceCacher>(
      profile_, base::BindOnce(EnumerateResultToBool, std::move(callback)));
  device::WinWebAuthnApiAuthenticator::EnumeratePlatformCredentials(
      api_, base::BindOnce(&CredentialPresenceCacher::OnEnumerateResult,
                           std::move(cacher)));
}

void LocalCredentialManagementWin::Enumerate(
    base::OnceCallback<void(
        std::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
        callback) {
  if (!api_->IsAvailable() || !api_->SupportsSilentDiscovery()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  auto cacher =
      std::make_unique<CredentialPresenceCacher>(profile_, std::move(callback));
  device::WinWebAuthnApiAuthenticator::EnumeratePlatformCredentials(
      api_, base::BindOnce(&CredentialPresenceCacher::OnEnumerateResult,
                           std::move(cacher)));
}

void LocalCredentialManagementWin::Delete(
    base::span<const uint8_t> credential_id,
    base::OnceCallback<void(bool)> callback) {
  device::WinWebAuthnApiAuthenticator::DeletePlatformCredential(
      api_, credential_id, std::move(callback));
}

void LocalCredentialManagementWin::Edit(
    base::span<uint8_t> credential_id,
    std::string new_username,
    base::OnceCallback<void(bool)> callback) {
  // Editing passkeys should not be an option in Windows.
  NOTREACHED_IN_MIGRATION();
}
