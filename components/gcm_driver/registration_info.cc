// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/registration_info.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace gcm {

namespace {
constexpr char kInstanceIDSerializationPrefix[] = "iid-";
constexpr char kSerializedValidationTimeSeparator = '#';
constexpr char kSerializedKeySeparator = ',';
constexpr int kInstanceIDSerializationPrefixLength =
    sizeof(kInstanceIDSerializationPrefix) / sizeof(char) - 1;
}  // namespace

// static
scoped_refptr<RegistrationInfo> RegistrationInfo::BuildFromString(
    const std::string& serialized_key,
    const std::string& serialized_value,
    std::string* registration_id) {
  scoped_refptr<RegistrationInfo> registration;

  if (base::StartsWith(serialized_key, kInstanceIDSerializationPrefix,
                       base::CompareCase::SENSITIVE)) {
    registration = base::MakeRefCounted<InstanceIDTokenInfo>();
  } else {
    registration = base::MakeRefCounted<GCMRegistrationInfo>();
  }

  if (!registration->Deserialize(serialized_key, serialized_value,
                                 registration_id)) {
    registration.reset();
  }
  return registration;
}

RegistrationInfo::RegistrationInfo() = default;

RegistrationInfo::~RegistrationInfo() = default;

// static
const GCMRegistrationInfo* GCMRegistrationInfo::FromRegistrationInfo(
    const RegistrationInfo* registration_info) {
  if (!registration_info || registration_info->GetType() != GCM_REGISTRATION)
    return nullptr;
  return static_cast<const GCMRegistrationInfo*>(registration_info);
}

// static
GCMRegistrationInfo* GCMRegistrationInfo::FromRegistrationInfo(
    RegistrationInfo* registration_info) {
  if (!registration_info || registration_info->GetType() != GCM_REGISTRATION)
    return nullptr;
  return static_cast<GCMRegistrationInfo*>(registration_info);
}

GCMRegistrationInfo::GCMRegistrationInfo() = default;

GCMRegistrationInfo::~GCMRegistrationInfo() = default;

RegistrationInfo::RegistrationType GCMRegistrationInfo::GetType() const {
  return GCM_REGISTRATION;
}

std::string GCMRegistrationInfo::GetSerializedKey() const {
  // Multiple registrations are not supported for legacy GCM. So the key is
  // purely based on the application id.
  return app_id;
}

std::string GCMRegistrationInfo::GetSerializedValue(
    const std::string& registration_id) const {
  if (sender_ids.empty() || registration_id.empty())
    return std::string();

  // Serialize as:
  // sender1,sender2,...=reg_id#time_of_last_validation
  std::string value;
  for (auto iter = sender_ids.begin(); iter != sender_ids.end(); ++iter) {
    DCHECK(!iter->empty() &&
           iter->find(',') == std::string::npos &&
           iter->find('=') == std::string::npos);
    if (!value.empty())
      value += ",";
    value += *iter;
  }

  return base::StringPrintf("%s=%s%c%" PRId64, value.c_str(),
                            registration_id.c_str(),
                            kSerializedValidationTimeSeparator,
                            last_validated.since_origin().InMicroseconds());
}

bool GCMRegistrationInfo::Deserialize(const std::string& serialized_key,
                                      const std::string& serialized_value,
                                      std::string* registration_id) {
  if (serialized_key.empty() || serialized_value.empty())
    return false;

  // Application ID is same as the serialized key.
  app_id = serialized_key;

  // Sender IDs and registration ID are constructed from the serialized value.
  size_t pos_equals = serialized_value.find('=');
  if (pos_equals == std::string::npos)
    return false;
  // Note that it's valid for pos_hash to be std::string::npos.
  size_t pos_hash = serialized_value.find(kSerializedValidationTimeSeparator);
  bool has_timestamp = pos_hash != std::string::npos;

  std::string senders = serialized_value.substr(0, pos_equals);
  std::string registration_id_str, last_validated_str;
  if (has_timestamp) {
    registration_id_str =
        serialized_value.substr(pos_equals + 1, pos_hash - pos_equals - 1);
    last_validated_str = serialized_value.substr(pos_hash + 1);
  } else {
    registration_id_str = serialized_value.substr(pos_equals + 1);
  }

  sender_ids = base::SplitString(
      senders, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (sender_ids.empty() || registration_id_str.empty()) {
    sender_ids.clear();
    registration_id_str.clear();
    return false;
  }

  if (registration_id)
    *registration_id = registration_id_str;
  int64_t last_validated_ms = 0;
  if (base::StringToInt64(last_validated_str, &last_validated_ms)) {
    // It's okay for |last_validated| to be the default base::Time() value
    // when there is no serialized timestamp value available.
    last_validated = base::Time() + base::Microseconds(last_validated_ms);
  }

  return true;
}

// static
const InstanceIDTokenInfo* InstanceIDTokenInfo::FromRegistrationInfo(
    const RegistrationInfo* registration_info) {
  if (!registration_info || registration_info->GetType() != INSTANCE_ID_TOKEN)
    return nullptr;
  return static_cast<const InstanceIDTokenInfo*>(registration_info);
}

// static
InstanceIDTokenInfo* InstanceIDTokenInfo::FromRegistrationInfo(
    RegistrationInfo* registration_info) {
  if (!registration_info || registration_info->GetType() != INSTANCE_ID_TOKEN)
    return nullptr;
  return static_cast<InstanceIDTokenInfo*>(registration_info);
}

InstanceIDTokenInfo::InstanceIDTokenInfo() = default;

InstanceIDTokenInfo::~InstanceIDTokenInfo() = default;

RegistrationInfo::RegistrationType InstanceIDTokenInfo::GetType() const {
  return INSTANCE_ID_TOKEN;
}

std::string InstanceIDTokenInfo::GetSerializedKey() const {
  DCHECK(app_id.find(',') == std::string::npos &&
         authorized_entity.find(',') == std::string::npos &&
         scope.find(',') == std::string::npos);

  // Multiple registrations are supported for Instance ID. So the key is based
  // on the combination of (app_id, authorized_entity, scope).

  // Adds a prefix to differentiate easily with GCM registration key.
  return base::StringPrintf("%s%s%c%s%c%s", kInstanceIDSerializationPrefix,
                            app_id.c_str(), kSerializedKeySeparator,
                            authorized_entity.c_str(), kSerializedKeySeparator,
                            scope.c_str());
}

std::string InstanceIDTokenInfo::GetSerializedValue(
    const std::string& registration_id) const {
  int64_t last_validated_ms = last_validated.since_origin().InMicroseconds();
  return registration_id + kSerializedValidationTimeSeparator +
         base::NumberToString(last_validated_ms);
}

bool InstanceIDTokenInfo::Deserialize(const std::string& serialized_key,
                                      const std::string& serialized_value,
                                      std::string* registration_id) {
  if (serialized_key.empty() || serialized_value.empty())
    return false;

  if (!base::StartsWith(serialized_key, kInstanceIDSerializationPrefix,
                        base::CompareCase::SENSITIVE))
    return false;

  std::vector<std::string> fields = base::SplitString(
      serialized_key.substr(kInstanceIDSerializationPrefixLength), ",",
      base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (fields.size() != 3 || fields[0].empty() ||
      fields[1].empty() || fields[2].empty()) {
    return false;
  }
  app_id = fields[0];
  authorized_entity = fields[1];
  scope = fields[2];

  // Get Registration ID and last_validated from serialized value
  size_t pos_hash = serialized_value.find(kSerializedValidationTimeSeparator);
  bool has_timestamp = (pos_hash != std::string::npos);

  std::string registration_id_str, last_validated_str;
  if (has_timestamp) {
    registration_id_str = serialized_value.substr(0, pos_hash);
    last_validated_str = serialized_value.substr(pos_hash + 1);
  } else {
    registration_id_str = serialized_value;
  }

  if (registration_id)
    *registration_id = registration_id_str;

  int64_t last_validated_ms = 0;
  if (base::StringToInt64(last_validated_str, &last_validated_ms)) {
    // It's okay for last_validated to be the default base::Time() value
    // when there is no serialized timestamp available.
    last_validated += base::Microseconds(last_validated_ms);
  }

  return true;
}

bool RegistrationInfoComparer::operator()(
    const scoped_refptr<RegistrationInfo>& a,
    const scoped_refptr<RegistrationInfo>& b) const {
  DCHECK(a.get() && b.get());

  // For GCMRegistrationInfo, the comparison is based on app_id only.
  // For InstanceIDTokenInfo, the comparison is based on
  // <app_id, authorized_entity, scope>.
  if (a->app_id < b->app_id)
    return true;
  if (a->app_id > b->app_id)
    return false;

  InstanceIDTokenInfo* iid_a =
      InstanceIDTokenInfo::FromRegistrationInfo(a.get());
  InstanceIDTokenInfo* iid_b =
    InstanceIDTokenInfo::FromRegistrationInfo(b.get());

  // !iid_a && !iid_b => false.
  // !iid_a && iid_b => true.
  // This makes GCM record is sorted before InstanceID record.
  if (!iid_a)
    return iid_b != nullptr;

  // iid_a && !iid_b => false.
  if (!iid_b)
    return false;

  // Otherwise, compare with authorized_entity and scope.
  if (iid_a->authorized_entity < iid_b->authorized_entity)
    return true;
  if (iid_a->authorized_entity > iid_b->authorized_entity)
    return false;
  return iid_a->scope < iid_b->scope;
}

bool ExistsGCMRegistrationInMap(const RegistrationInfoMap& map,
                                const std::string& app_id) {
  scoped_refptr<RegistrationInfo> gcm_registration =
      base::MakeRefCounted<GCMRegistrationInfo>();
  gcm_registration->app_id = app_id;
  return map.find(gcm_registration) != map.end();
}

}  // namespace gcm
