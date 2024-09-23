// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/cablev2_devices.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/features.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

using device::cablev2::Pairing;

namespace cablev2 {

namespace {

template <size_t N>
bool CopyBytestring(std::array<uint8_t, N>* out, const std::string* value) {
  if (!value) {
    return false;
  }

  std::string bytes;
  if (!base::Base64Decode(*value, &bytes) || bytes.size() != N) {
    return false;
  }

  base::ranges::copy(bytes, out->begin());
  return true;
}

bool CopyBytestring(std::vector<uint8_t>* out, const std::string* value) {
  if (!value) {
    return false;
  }

  std::string bytes;
  if (!base::Base64Decode(*value, &bytes)) {
    return false;
  }

  out->clear();
  out->insert(out->begin(), bytes.begin(), bytes.end());
  return true;
}

bool CopyString(std::string* out, const std::string* value) {
  if (!value) {
    return false;
  }
  *out = *value;
  return true;
}

const char kWebAuthnCablePairingsPrefName[] = "webauthn.cablev2_pairings";

// The `kWebAuthnCablePairingsPrefName` preference contains a list of dicts,
// where each dict has these keys:
const char kPairingPrefName[] = "name";
const char kPairingPrefContactId[] = "contact_id";
// This used to be "tunnel_server" and contain the decoded domain as a string.
const char kPairingPrefEncodedTunnelServer[] = "encoded_tunnel_server";
const char kPairingPrefId[] = "id";
const char kPairingPrefSecret[] = "secret";
const char kPairingPrefPublicKey[] = "pub_key";
const char kPairingPrefTime[] = "time";
const char kPairingPrefNewImpl[] = "new_impl";

// NameForDisplay removes line-breaking characters from `raw_name` to ensure
// that the transport-selection UI isn't too badly broken by nonsense names.
static std::string NameForDisplay(std::string_view raw_name) {
  std::u16string unicode_name = base::UTF8ToUTF16(raw_name);
  std::u16string_view trimmed_name =
      base::TrimWhitespace(unicode_name, base::TRIM_ALL);
  // These are all the Unicode mandatory line-breaking characters
  // (https://www.unicode.org/reports/tr14/tr14-32.html#Properties).
  constexpr char16_t kLineTerminators[] = {0x0a, 0x0b, 0x0c, 0x0d, 0x85, 0x2028,
                                           0x2029,
                                           // Array must be NUL terminated.
                                           0};
  std::u16string nonbreaking_name;
  base::RemoveChars(trimmed_name, kLineTerminators, &nonbreaking_name);
  return base::UTF16ToUTF8(nonbreaking_name);
}

// DeletePairingByPublicKey erases any pairing with the given public key
// from `list`.
void DeletePairingByPublicKey(base::Value::List& list,
                              const std::string& public_key_base64) {
  list.EraseIf([&public_key_base64](const auto& value) {
    if (!value.is_dict()) {
      return false;
    }
    const std::string* pref_public_key =
        value.GetDict().FindString(kPairingPrefPublicKey);
    return pref_public_key && *pref_public_key == public_key_base64;
  });
}

std::vector<std::unique_ptr<Pairing>> GetSyncedDevices(Profile* const profile) {
  std::vector<std::unique_ptr<Pairing>> ret;
  syncer::DeviceInfoSyncService* const sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return ret;
  }

  syncer::DeviceInfoTracker* const tracker =
      sync_service->GetDeviceInfoTracker();
  std::vector<const syncer::DeviceInfo*> devices = tracker->GetAllDeviceInfo();

  const base::Time now = base::Time::Now();
  for (const syncer::DeviceInfo* device : devices) {
    std::unique_ptr<Pairing> pairing = PairingFromSyncedDevice(device, now);
    if (!pairing) {
      continue;
    }
    ret.emplace_back(std::move(pairing));
  }

  return ret;
}

std::vector<std::unique_ptr<Pairing>> GetLinkedDevices(Profile* const profile) {
  PrefService* const prefs = profile->GetPrefs();
  const base::Value::List& pref_pairings =
      prefs->GetList(kWebAuthnCablePairingsPrefName);

  std::vector<std::unique_ptr<Pairing>> ret;
  for (const auto& pairing : pref_pairings) {
    if (!pairing.is_dict()) {
      continue;
    }

    const base::Value::Dict& dict = pairing.GetDict();
    auto out_pairing = std::make_unique<Pairing>();
    if (!CopyString(&out_pairing->name, dict.FindString(kPairingPrefName)) ||
        !CopyBytestring(&out_pairing->contact_id,
                        dict.FindString(kPairingPrefContactId)) ||
        !CopyBytestring(&out_pairing->id, dict.FindString(kPairingPrefId)) ||
        !CopyBytestring(&out_pairing->secret,
                        dict.FindString(kPairingPrefSecret)) ||
        !CopyBytestring(&out_pairing->peer_public_key_x962,
                        dict.FindString(kPairingPrefPublicKey))) {
      continue;
    }

    const std::optional<bool> is_new_impl = dict.FindBool(kPairingPrefNewImpl);
    out_pairing->from_new_implementation = is_new_impl && *is_new_impl;
    out_pairing->name = NameForDisplay(out_pairing->name);
    const std::optional<uint16_t> maybe_tunnel_server =
        dict.FindInt(kPairingPrefEncodedTunnelServer);
    if (maybe_tunnel_server) {
      std::optional<device::cablev2::tunnelserver::KnownDomainID>
          maybe_domain_id = device::cablev2::tunnelserver::ToKnownDomainID(
              *maybe_tunnel_server);
      if (!maybe_domain_id) {
        continue;
      }
      out_pairing->tunnel_server_domain = *maybe_domain_id;
    } else {
      // Pairings stored before we started tracking the encoded tunnel server
      // domain are known to be Android phones.
      out_pairing->tunnel_server_domain = device::cablev2::kTunnelServer;
    }
    ret.emplace_back(std::move(out_pairing));
  }

  return ret;
}

// FindUniqueName checks whether |name| is already contained in |existing_names|
// (after projecting with |NameForDisplay|). If so it appends a counter so that
// it isn't.
std::string FindUniqueName(const std::string& orig_name,
                           base::span<const std::string_view> existing_names) {
  std::string name = orig_name;
  std::string name_for_display = NameForDisplay(name);
  for (int i = 1;; i++) {
    if (!base::Contains(existing_names, name_for_display, &NameForDisplay)) {
      // The new name is unique.
      break;
    }

    // The new name collides with an existing one. Append a counter to the
    // original and try again. (If the original string is right-to-left then
    // the counter will appear on the left, not the right, of the string even
    // though the codepoints are appended here.)
    name = orig_name + " (" + base::NumberToString(i) + ")";
    name_for_display = NameForDisplay(name);
  }

  return name;
}

}  // namespace

// PairingFromSyncedDevice extracts the caBLEv2 information from Sync's
// DeviceInfo (if any) into a caBLEv2 pairing. It may return nullptr.
std::unique_ptr<Pairing> PairingFromSyncedDevice(
    const syncer::DeviceInfo* device,
    const base::Time& now) {
  const std::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>&
      maybe_paask_info = device->paask_info();
  if (!maybe_paask_info) {
    return nullptr;
  }
  const syncer::DeviceInfo::PhoneAsASecurityKeyInfo& paask_info =
      *maybe_paask_info;

  if (device::cablev2::sync::IDIsMoreThanNPeriodsOld(
          paask_info.id, device::cablev2::kMaxSyncInfoDaysForConsumer)) {
    // Old entries are dropped as phones won't honor linking information that is
    // excessively old.
    return nullptr;
  }

  auto pairing = std::make_unique<Pairing>();
  pairing->from_sync_deviceinfo = true;
  pairing->name = NameForDisplay(device->client_name());

  const std::optional<device::cablev2::tunnelserver::KnownDomainID>
      tunnel_server_domain = device::cablev2::tunnelserver::ToKnownDomainID(
          paask_info.tunnel_server_domain);
  if (!tunnel_server_domain) {
    // It's possible that a phone is running a more modern version of Chrome
    // and uses an assigned tunnel server domain that is unknown to this code.
    return nullptr;
  }

  pairing->tunnel_server_domain = *tunnel_server_domain;
  pairing->contact_id = paask_info.contact_id;
  pairing->peer_public_key_x962 = paask_info.peer_public_key_x962;
  pairing->secret.assign(paask_info.secret.begin(), paask_info.secret.end());
  pairing->last_updated = device->last_updated_timestamp();

  // The pairing ID from sync is zero-padded to the standard length.
  pairing->id.assign(device::cablev2::kPairingIDSize, 0);
  static_assert(device::cablev2::kPairingIDSize >= sizeof(paask_info.id), "");
  memcpy(pairing->id.data(), &paask_info.id, sizeof(paask_info.id));

  // The channel priority is only approximate and exists to help testing and
  // development. I.e. we want the development or Canary install on a device to
  // shadow the stable channel so that it's possible to test things. This code
  // is matching the string generated by `FormatUserAgentForSync`.
  const std::string& user_agent = device->sync_user_agent();
  if (user_agent.find("-devel") != std::string::npos) {
    pairing->channel_priority = 5;
  } else if (user_agent.find("(canary)") != std::string::npos) {
    pairing->channel_priority = 4;
  } else if (user_agent.find("(dev)") != std::string::npos) {
    pairing->channel_priority = 3;
  } else if (user_agent.find("(beta)") != std::string::npos) {
    pairing->channel_priority = 2;
  } else if (user_agent.find("(stable)") != std::string::npos) {
    pairing->channel_priority = 1;
  } else {
    pairing->channel_priority = 0;
  }

  return pairing;
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kWebAuthnCablePairingsPrefName,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

KnownDevices::KnownDevices() = default;
KnownDevices::~KnownDevices() = default;

// static
std::unique_ptr<KnownDevices> KnownDevices::FromProfile(Profile* profile) {
  if (profile->IsOffTheRecord()) {
    // For Incognito windows we collect the devices from the parent profile.
    // The `AuthenticatorRequestDialogController` will notice that it's an OTR
    // profile and display a confirmation interstitial for makeCredential calls.
    profile = profile->GetOriginalProfile();
  }

  auto ret = std::make_unique<KnownDevices>();
  ret->synced_devices = GetSyncedDevices(profile);
  ret->linked_devices = GetLinkedDevices(profile);
  return ret;
}

std::vector<std::unique_ptr<Pairing>> MergeDevices(
    std::unique_ptr<KnownDevices> known_devices,
    const icu::Locale* locale) {
  std::vector<std::unique_ptr<Pairing>> ret;
  ret.swap(known_devices->synced_devices);
  std::sort(ret.begin(), ret.end(), Pairing::CompareByMostRecentFirst);

  for (auto& pairing : known_devices->linked_devices) {
    ret.emplace_back(std::move(pairing));
  }

  // All the pairings from sync come first in `ret`, sorted by most recent
  // first, followed by pairings from prefs, which are known to have unique
  // public keys within themselves. A stable sort by public key will group
  // together any pairings for the same Chrome instance, preferring recent sync
  // records, then `std::unique` will delete all but the first.
  std::stable_sort(ret.begin(), ret.end(), Pairing::CompareByPublicKey);
  ret.erase(std::unique(ret.begin(), ret.end(), Pairing::EqualPublicKeys),
            ret.end());

  // ret now contains only a single entry per Chrome install. There can still be
  // multiple entries for a given name, however. Sort by most recent and then by
  // channel. That means that, considering all the entries for a given name,
  // Sync entries on unstable channels have top priority. Within a given
  // channel, the most recent entry has priority.

  std::sort(ret.begin(), ret.end(), Pairing::CompareByMostRecentFirst);
  std::stable_sort(ret.begin(), ret.end(),
                   Pairing::CompareByLeastStableChannelFirst);

  // Last, sort by name while preserving the order of entries with the same
  // name. Need to make a reference for Compare, because libstdc++ would
  // need copy constructor otherwise.
  Pairing::NameComparator comparator = Pairing::CompareByName(locale);
  std::stable_sort(ret.begin(), ret.end(), std::ref(comparator));

  return ret;
}

std::vector<std::string_view> KnownDevices::Names() const {
  std::vector<std::string_view> names;
  names.reserve(this->synced_devices.size() + this->linked_devices.size());
  for (const std::unique_ptr<device::cablev2::Pairing>& device :
       this->synced_devices) {
    names.push_back(device->name);
  }
  for (const std::unique_ptr<device::cablev2::Pairing>& device :
       this->linked_devices) {
    names.push_back(device->name);
  }
  return names;
}

void AddPairing(Profile* profile, std::unique_ptr<Pairing> pairing) {
  // This is called when doing a QR-code pairing with a phone and the phone
  // sends long-term pairing information during the handshake. The pairing
  // information is saved in preferences for future operations.
  ScopedListPrefUpdate update(profile->GetPrefs(),
                              kWebAuthnCablePairingsPrefName);

  // Find any existing entries with the same public key and replace them. The
  // handshake protocol requires the phone to prove possession of the public
  // key so it's not possible for an evil phone to displace another's pairing.
  std::string public_key_base64 =
      base::Base64Encode(pairing->peer_public_key_x962);
  DeletePairingByPublicKey(*update, public_key_base64);

  // As an exception to the above rule, we allow a new pairing to replace a
  // previous one with the same name if the new pairing is from the new
  // implementation and the old one isn't. When we transition the implementation
  // on Android to the new implementation, it's not feasible to port the
  // identity key across. Rather than have duplicate pairings, we allow this
  // replacement once.
  //
  // TODO(crbug.com/40910325): remove once the transition is firmly complete.
  // Probably by May 2024.
  const std::string& claimed_name = pairing->name;
  if (pairing->from_new_implementation) {
    update->EraseIf([&claimed_name](const auto& value) {
      if (!value.is_dict()) {
        return false;
      }
      const std::optional<bool> pref_new_impl =
          value.GetDict().FindBool(kPairingPrefNewImpl);
      const std::string* const pref_name =
          value.GetDict().FindString(kPairingPrefName);
      return pref_name && *pref_name == claimed_name &&
             (!pref_new_impl || !*pref_new_impl);
    });
  }

  base::Value::Dict dict;
  dict.Set(kPairingPrefPublicKey, std::move(public_key_base64));
  dict.Set(kPairingPrefEncodedTunnelServer,
           pairing->tunnel_server_domain.value());
  // `Names` is called without calling `MergeDevices` because that function will
  // discard linked entries with duplicate public keys, which can hide some
  // names that we would still like to avoid colliding with.
  dict.Set(kPairingPrefName,
           FindUniqueName(pairing->name,
                          KnownDevices::FromProfile(profile)->Names()));
  dict.Set(kPairingPrefContactId, base::Base64Encode(pairing->contact_id));
  dict.Set(kPairingPrefId, base::Base64Encode(pairing->id));
  dict.Set(kPairingPrefSecret, base::Base64Encode(pairing->secret));
  if (pairing->from_new_implementation) {
    dict.Set(kPairingPrefNewImpl, true);
  }

  dict.Set(kPairingPrefTime, base::UnlocalizedTimeFormatWithPattern(
                                 base::Time::Now(), "yyyy-MM-dd'T'HH:mm:ssX",
                                 icu::TimeZone::getGMT()));

  update->Append(std::move(dict));
}

// DeletePairingByPublicKey erases any pairing with the given public key
// from `list`.
void DeletePairingByPublicKey(
    PrefService* pref_service,
    std::array<uint8_t, device::kP256X962Length> public_key) {
  ScopedListPrefUpdate update(pref_service, kWebAuthnCablePairingsPrefName);
  DeletePairingByPublicKey(*update, base::Base64Encode(public_key));
}

bool RenamePairing(
    PrefService* pref_service,
    const std::array<uint8_t, device::kP256X962Length>& public_key,
    const std::string& new_name,
    base::span<const std::string_view> existing_names) {
  const std::string name = FindUniqueName(new_name, existing_names);
  const std::string public_key_base64 = base::Base64Encode(public_key);

  ScopedListPrefUpdate update(pref_service, kWebAuthnCablePairingsPrefName);

  for (base::Value& value : *update) {
    if (!value.is_dict()) {
      continue;
    }

    base::Value::Dict& dict = value.GetDict();
    const std::string* pref_public_key = dict.FindString(kPairingPrefPublicKey);
    if (pref_public_key && *pref_public_key == public_key_base64) {
      dict.Set(kPairingPrefName, std::move(name));
      return true;
    }
  }

  return false;
}

}  // namespace cablev2
