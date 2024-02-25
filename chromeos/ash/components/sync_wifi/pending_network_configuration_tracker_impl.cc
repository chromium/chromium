// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync_wifi/pending_network_configuration_tracker_impl.h"

#include <optional>

#include "base/base64url.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::sync_wifi {

namespace {

const char kPendingNetworkConfigurationsPref[] =
    "sync_wifi.pending_network_configuration_updates";
const char kChangeGuidKey[] = "ChangeGuid";
const char kCompletedAttemptsKey[] = "CompletedAttempts";
const char kSpecificsKey[] = "Specifics";

std::string GeneratePath(const NetworkIdentifier& id,
                         const std::string& subkey) {
  return base::StringPrintf("%s.%s", id.SerializeToString().c_str(),
                            subkey.c_str());
}

PendingNetworkConfigurationUpdate ConvertToPendingUpdate(
    base::Value::Dict* dict,
    const NetworkIdentifier& id) {
  std::string* change_guid = dict->FindString(kChangeGuidKey);
  std::optional<sync_pb::WifiConfigurationSpecifics> specifics;
  std::string* encoded_specifics_string = dict->FindString(kSpecificsKey);
  std::string specifics_string;
  if (encoded_specifics_string &&
      base::Base64UrlDecode(*encoded_specifics_string,
                            base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                            &specifics_string) &&
      !specifics_string.empty()) {
    sync_pb::WifiConfigurationSpecifics data;
    data.ParseFromString(specifics_string);
    specifics = data;
  }
  std::optional<int> completed_attempts = dict->FindInt(kCompletedAttemptsKey);

  DCHECK(change_guid);
  DCHECK(completed_attempts);

  return PendingNetworkConfigurationUpdate(id, *change_guid, specifics,
                                           completed_attempts.value());
}

}  // namespace

// static
void PendingNetworkConfigurationTrackerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kPendingNetworkConfigurationsPref);
}

PendingNetworkConfigurationTrackerImpl::PendingNetworkConfigurationTrackerImpl(
    PrefService* pref_service)
    : pref_service_(pref_service),
      dict_(pref_service_->GetDict(kPendingNetworkConfigurationsPref).Clone()) {
}

PendingNetworkConfigurationTrackerImpl::
    ~PendingNetworkConfigurationTrackerImpl() = default;

std::string PendingNetworkConfigurationTrackerImpl::TrackPendingUpdate(
    const NetworkIdentifier& id,
    const std::optional<sync_pb::WifiConfigurationSpecifics>& specifics) {
  std::string serialized_specifics;
  if (!specifics)
    serialized_specifics = std::string();
  else
    CHECK(specifics->SerializeToString(&serialized_specifics));

  // base::Value only allows UTF8 encoded strings.
  std::string encoded_specifics;
  base::Base64UrlEncode(serialized_specifics,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_specifics);
  std::string change_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  dict_.SetByDottedPath(GeneratePath(id, kChangeGuidKey), change_guid);
  dict_.SetByDottedPath(GeneratePath(id, kSpecificsKey), encoded_specifics);
  dict_.SetByDottedPath(GeneratePath(id, kCompletedAttemptsKey), 0);
  pref_service_->SetDict(kPendingNetworkConfigurationsPref, dict_.Clone());

  return change_guid;
}

void PendingNetworkConfigurationTrackerImpl::MarkComplete(
    const std::string& change_guid,
    const NetworkIdentifier& id) {
  if (!GetPendingUpdate(change_guid, id))
    return;

  dict_.Remove(id.SerializeToString());
  pref_service_->SetDict(kPendingNetworkConfigurationsPref, dict_.Clone());
}

void PendingNetworkConfigurationTrackerImpl::IncrementCompletedAttempts(
    const std::string& change_guid,
    const NetworkIdentifier& id) {
  std::string path = GeneratePath(id, kCompletedAttemptsKey);
  int completed_attempts = dict_.FindIntByDottedPath(path).value_or(0);
  dict_.SetByDottedPath(path, completed_attempts + 1);
}

std::vector<PendingNetworkConfigurationUpdate>
PendingNetworkConfigurationTrackerImpl::GetPendingUpdates() {
  std::vector<PendingNetworkConfigurationUpdate> list;
  for (const auto [key, value] : dict_) {
    list.push_back(ConvertToPendingUpdate(
        /*dict=*/&value.GetDict(),
        NetworkIdentifier::DeserializeFromString(key)));
  }
  return list;
}
std::optional<PendingNetworkConfigurationUpdate>
PendingNetworkConfigurationTrackerImpl::GetPendingUpdate(
    const std::string& change_guid,
    const NetworkIdentifier& id) {
  std::string* found_id =
      dict_.FindStringByDottedPath(GeneratePath(id, kChangeGuidKey));
  if (!found_id || *found_id != change_guid)
    return std::nullopt;

  return ConvertToPendingUpdate(dict_.FindDict(id.SerializeToString()), id);
}

}  // namespace ash::sync_wifi
