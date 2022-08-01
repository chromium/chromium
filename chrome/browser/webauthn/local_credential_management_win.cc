// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/local_credential_management.h"

#include <algorithm>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/i18n/string_compare.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/common/content_features.h"
#include "device/fido/win/authenticator.h"
#include "device/fido/win/webauthn_api.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace {

// CredentialComparator compares two credentials based on their RP ID's eTLD +
// 1, then on the label-reversed RP ID, then on user.name, and finally on
// credential ID if the previous values are equal.
class CredentialComparator {
 public:
  CredentialComparator() {
    UErrorCode error = U_ZERO_ERROR;
    collator_.reset(
        icu::Collator::createInstance(icu::Locale::getDefault(), error));
  }

  bool operator()(const device::DiscoverableCredentialMetadata& a,
                  const device::DiscoverableCredentialMetadata& b) {
    UCollationResult relation = base::i18n::CompareString16WithCollator(
        *collator_, ETLDPlus1(a.rp_id), ETLDPlus1(b.rp_id));
    if (relation != UCOL_EQUAL) {
      return relation == UCOL_LESS;
    }

    relation = base::i18n::CompareString16WithCollator(
        *collator_, LabelReverse(a.rp_id), LabelReverse(b.rp_id));
    if (relation != UCOL_EQUAL) {
      return relation == UCOL_LESS;
    }

    relation = base::i18n::CompareString16WithCollator(
        *collator_, base::UTF8ToUTF16(a.user.name.value_or("")),
        base::UTF8ToUTF16(b.user.name.value_or("")));
    if (relation != UCOL_EQUAL) {
      return relation == UCOL_LESS;
    }

    return a.cred_id < b.cred_id;
  }

 private:
  static std::u16string ETLDPlus1(const std::string& rp_id) {
    std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
        rp_id, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (domain.empty()) {
      domain = rp_id;
    }
    return base::UTF8ToUTF16(domain);
  }

  static std::u16string LabelReverse(const std::string& rp_id) {
    std::vector<base::StringPiece> parts = base::SplitStringPiece(
        rp_id, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::reverse(parts.begin(), parts.end());
    return base::UTF8ToUTF16(base::JoinString(parts, "."));
  }

  std::unique_ptr<icu::Collator> collator_;
};

bool ContainsUserCreatedCredential(
    const std::vector<device::DiscoverableCredentialMetadata> credentials) {
  return std::any_of(
      credentials.begin(), credentials.end(),
      [](const device::DiscoverableCredentialMetadata& cred) -> bool {
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
          absl::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
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
      absl::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
      callback_;
};

void EnumerateResultToBool(
    base::OnceCallback<void(bool)> callback,
    absl::optional<std::vector<device::DiscoverableCredentialMetadata>>
        credentials) {
  std::move(callback).Run(credentials.has_value() &&
                          ContainsUserCreatedCredential(*credentials));
}

}  // namespace

LocalCredentialManagement::LocalCredentialManagement(
    device::WinWebAuthnApi* api)
    : api_(api) {}

void LocalCredentialManagement::HasCredentials(
    Profile* profile,
    base::OnceCallback<void(bool)> callback) {
  absl::optional<bool> result;

  if (!api_->IsAvailable() || !api_->SupportsSilentDiscovery() ||
      !base::FeatureList::IsEnabled(features::kWebAuthConditionalUI)) {
    result = false;
  } else if (profile->GetPrefs()->GetBoolean(kHasPlatformCredentialsPref)) {
    result = true;
  }

  if (result.has_value()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), *result));
    return;
  }

  auto cacher = std::make_unique<CredentialPresenceCacher>(
      profile, base::BindOnce(EnumerateResultToBool, std::move(callback)));
  device::WinWebAuthnApiAuthenticator::EnumeratePlatformCredentials(
      api_, base::BindOnce(&CredentialPresenceCacher::OnEnumerateResult,
                           std::move(cacher)));
}

void LocalCredentialManagement::Enumerate(
    Profile* profile,
    base::OnceCallback<void(
        absl::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
        callback) {
  if (!api_->IsAvailable() || !api_->SupportsSilentDiscovery()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  auto cacher =
      std::make_unique<CredentialPresenceCacher>(profile, std::move(callback));
  device::WinWebAuthnApiAuthenticator::EnumeratePlatformCredentials(
      api_, base::BindOnce(&CredentialPresenceCacher::OnEnumerateResult,
                           std::move(cacher)));
}

void LocalCredentialManagement::Delete(
    Profile* profile,
    base::span<const uint8_t> credential_id,
    base::OnceCallback<void(bool)> callback) {
  device::WinWebAuthnApiAuthenticator::DeletePlatformCredential(
      api_, credential_id, std::move(callback));
}

// static
void LocalCredentialManagement::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kHasPlatformCredentialsPref, false);
}
