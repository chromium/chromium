// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_profile.h"

#include "base/strings/utf_string_conversions.h"
#include "dbus/object_path.h"

namespace ash {
namespace {

// Keys used by ToDictionaryValue() and FromDictionaryValue().
const char kKeyState[] = "State";
const char kKeyPath[] = "Path";
const char kKeyEid[] = "Eid";
const char kKeyIccid[] = "Iccid";
const char kKeyName[] = "Name";
const char kKeyNickname[] = "Nickname";
const char kKeyServiceProvider[] = "ServiceProvider";
const char kKeyActivationCode[] = "ActivationCode";

}  // namespace

// static
absl::optional<CellularESimProfile> CellularESimProfile::FromDictionaryValue(
    const base::Value& value) {
  if (!value.is_dict())
    return absl::nullopt;

  const std::string* path = value.FindStringPath(kKeyPath);
  if (!path)
    return absl::nullopt;

  absl::optional<int> state = value.FindIntPath(kKeyState);
  if (!state)
    return absl::nullopt;

  const std::string* eid = value.FindStringPath(kKeyEid);
  if (!eid)
    return absl::nullopt;

  const std::string* iccid = value.FindStringPath(kKeyIccid);
  if (!iccid)
    return absl::nullopt;

  const std::string* name = value.FindStringPath(kKeyName);
  if (!name)
    return absl::nullopt;

  const std::string* nickname = value.FindStringPath(kKeyNickname);
  if (!nickname)
    return absl::nullopt;

  const std::string* service_provider =
      value.FindStringPath(kKeyServiceProvider);
  if (!service_provider)
    return absl::nullopt;

  const std::string* activation_code = value.FindStringPath(kKeyActivationCode);
  if (!activation_code)
    return absl::nullopt;

  return CellularESimProfile(
      static_cast<State>(*state), dbus::ObjectPath(*path), *eid, *iccid,
      base::UTF8ToUTF16(*name), base::UTF8ToUTF16(*nickname),
      base::UTF8ToUTF16(*service_provider), *activation_code);
}

CellularESimProfile::CellularESimProfile(State state,
                                         const dbus::ObjectPath& path,
                                         const std::string& eid,
                                         const std::string& iccid,
                                         const std::u16string& name,
                                         const std::u16string& nickname,
                                         const std::u16string& service_provider,
                                         const std::string& activation_code)
    : state_(state),
      path_(path),
      eid_(eid),
      iccid_(iccid),
      name_(name),
      nickname_(nickname),
      service_provider_(service_provider),
      activation_code_(activation_code) {}

CellularESimProfile::CellularESimProfile(const CellularESimProfile&) = default;

CellularESimProfile& CellularESimProfile::operator=(
    const CellularESimProfile&) = default;

CellularESimProfile::~CellularESimProfile() = default;

base::Value CellularESimProfile::ToDictionaryValue() const {
  base::Value dictionary(base::Value::Type::DICT);
  dictionary.SetKey(kKeyState, base::Value(static_cast<int>(state_)));
  dictionary.SetKey(kKeyPath, base::Value(path_.value()));
  dictionary.SetKey(kKeyEid, base::Value(eid_));
  dictionary.SetKey(kKeyIccid, base::Value(iccid_));
  dictionary.SetKey(kKeyName, base::Value(name_));
  dictionary.SetKey(kKeyNickname, base::Value(nickname_));
  dictionary.SetKey(kKeyServiceProvider, base::Value(service_provider_));
  dictionary.SetKey(kKeyActivationCode, base::Value(activation_code_));
  return dictionary;
}

bool CellularESimProfile::operator==(const CellularESimProfile& other) const {
  return state_ == other.state_ && path_ == other.path_ && eid_ == other.eid_ &&
         iccid_ == other.iccid_ && name_ == other.name_ &&
         nickname_ == other.nickname_ &&
         service_provider_ == other.service_provider_ &&
         activation_code_ == other.activation_code_;
}

bool CellularESimProfile::operator!=(const CellularESimProfile& other) const {
  return !(*this == other);
}

}  // namespace ash
