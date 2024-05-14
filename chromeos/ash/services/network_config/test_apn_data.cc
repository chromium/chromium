// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/network_config/test_apn_data.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::network_config {

namespace {

namespace mojom = ::chromeos::network_config::mojom;

// TODO(b/162365553) Remove when shill constants are added.
constexpr char kShillApnId[] = "id";
constexpr char kShillApnAuthenticationType[] = "authentication_type";

bool IsPropertyEquals(const base::Value::Dict& apn,
                      const char* key,
                      const std::string& expected_value) {
  const std::string* actual_value = apn.FindString(key);
  if (!actual_value)
    return false;
  return expected_value == *actual_value;
}

}  //  namespace

TestApnData::TestApnData()
    : mojo_state(mojom::ApnState::kEnabled),
      onc_state(::onc::cellular_apn::kStateEnabled),
      mojo_authentication(mojom::ApnAuthenticationType::kAutomatic),
      onc_authentication(::onc::cellular_apn::kAuthenticationAutomatic),
      mojo_ip_type(mojom::ApnIpType::kAutomatic),
      onc_ip_type(::onc::cellular_apn::kIpTypeAutomatic),
      mojo_source(mojom::ApnSource::kUi),
      onc_source(::onc::cellular_apn::kSourceUi) {}

TestApnData::TestApnData(std::string access_point_name,
                         std::string name,
                         std::string username,
                         std::string password,
                         std::string attach,
                         std::string id,
                         mojom::ApnState mojo_state,
                         std::string onc_state,
                         mojom::ApnAuthenticationType mojo_authentication,
                         std::string onc_authentication,
                         mojom::ApnIpType mojo_ip_type,
                         std::string onc_ip_type,
                         mojom::ApnSource mojo_source,
                         std::string onc_source,
                         const std::vector<mojom::ApnType>& mojo_apn_types,
                         const std::vector<std::string>& onc_apn_types)
    : access_point_name(access_point_name),
      name(name),
      username(username),
      password(password),
      attach(attach),
      id(id),
      mojo_state(mojo_state),
      onc_state(onc_state),
      mojo_authentication(mojo_authentication),
      onc_authentication(onc_authentication),
      mojo_ip_type(mojo_ip_type),
      onc_ip_type(onc_ip_type),
      mojo_source(mojo_source),
      onc_source(onc_source),
      mojo_apn_types(mojo_apn_types),
      onc_apn_types(onc_apn_types) {}

TestApnData::~TestApnData() = default;

mojom::ApnPropertiesPtr TestApnData::AsMojoApn() const {
  auto apn = mojom::ApnProperties::New();
  apn->access_point_name = access_point_name;
  apn->name = name;
  apn->username = username;
  apn->password = password;
  apn->attach = attach;
  apn->authentication = mojo_authentication;
  if (features::IsApnRevampEnabled()) {
    apn->id = id.empty() ? std::nullopt : std::optional<std::string>(id);
    apn->ip_type = mojo_ip_type;
    apn->apn_types = mojo_apn_types;
    apn->state = mojo_state;
    apn->source = mojo_source;
  }
  return apn;
}

base::Value::Dict TestApnData::AsOncApn() const {
  base::Value::Dict apn;
  apn.Set(::onc::cellular_apn::kAccessPointName, access_point_name);
  apn.Set(::onc::cellular_apn::kName, name);
  apn.Set(::onc::cellular_apn::kUsername, username);
  apn.Set(::onc::cellular_apn::kPassword, password);
  apn.Set(::onc::cellular_apn::kAttach, attach);
  apn.Set(::onc::cellular_apn::kAuthentication, onc_authentication);
  if (features::IsApnRevampEnabled()) {
    apn.Set(::onc::cellular_apn::kId, id);
    apn.Set(::onc::cellular_apn::kState, onc_state);
    apn.Set(::onc::cellular_apn::kIpType, onc_ip_type);
    apn.Set(::onc::cellular_apn::kSource, onc_source);

    base::Value::List apn_types;
    for (const std::string& apn_type : onc_apn_types)
      apn_types.Append(apn_type);
    apn.Set(::onc::cellular_apn::kApnTypes, std::move(apn_types));
  }
  return apn;
}

base::Value::Dict TestApnData::AsShillApn() const {
  base::Value::Dict apn;
  apn.Set(shill::kApnProperty, access_point_name);
  apn.Set(shill::kApnNameProperty, name);
  apn.Set(shill::kApnUsernameProperty, username);
  apn.Set(shill::kApnPasswordProperty, password);
  apn.Set(shill::kApnAttachProperty, attach);
  apn.Set(shill::kApnSourceProperty, onc_source);
  apn.Set(kShillApnAuthenticationType, onc_authentication);
  if (features::IsApnRevampEnabled()) {
    apn.Set(kShillApnId, id);

    std::string shill_ip_type;
    if (onc_ip_type == ::onc::cellular_apn::kIpTypeIpv4) {
      shill_ip_type = shill::kApnIpTypeV4;
    } else if (onc_ip_type == ::onc::cellular_apn::kIpTypeIpv6) {
      shill_ip_type = shill::kApnIpTypeV6;
    } else if (onc_ip_type == shill::kApnIpTypeV4V6) {
      shill_ip_type = shill::kApnIpTypeV4V6;
    } else {
      NOTREACHED_IN_MIGRATION() << "An IP type is required";
    }
    apn.Set(shill::kApnIpTypeProperty, shill_ip_type);

    std::string apn_types;
    for (const std::string& apn_type : onc_apn_types) {
      if (apn_type == ::onc::cellular_apn::kApnTypeDefault) {
        apn_types += shill::kApnTypeDefault;
      } else if (apn_type == ::onc::cellular_apn::kApnTypeAttach) {
        apn_types += shill::kApnTypeIA;
      }
      apn_types += ",";
    }
    // Remove trailing comma.
    apn_types.pop_back();
    apn.Set(shill::kApnTypesProperty, base::Value(apn_types));
  }
  return apn;
}

std::string TestApnData::AsApnShillDict() const {
  // This will serialize the dictionary into valid JSON
  return AsShillApn().DebugString();
}

bool TestApnData::MojoApnEquals(const mojom::ApnProperties& apn) const {
  bool ret = access_point_name == apn.access_point_name;

  static auto MatchOptionalString =
      [](const std::string& expected,
         const std::optional<std::string>& actual) -> bool {
    if (actual.has_value())
      return expected == *actual;
    return expected.empty();
  };
  ret &= MatchOptionalString(name, apn.name);
  ret &= MatchOptionalString(username, apn.username);
  ret &= MatchOptionalString(password, apn.password);
  ret &= MatchOptionalString(attach, apn.attach);
  ret &= mojo_authentication == apn.authentication;

  if (features::IsApnRevampEnabled()) {
    ret &= mojo_ip_type == apn.ip_type;
    ret &= mojo_apn_types == apn.apn_types;
    ret &= mojo_source == apn.source;
  }
  return ret;
}

bool TestApnData::OncApnEquals(const base::Value::Dict& onc_apn,
                               bool has_state_field,
                               bool is_password_masked) const {
  bool ret = true;
  ret &= IsPropertyEquals(onc_apn, ::onc::cellular_apn::kAccessPointName,
                          access_point_name);
  ret &= IsPropertyEquals(onc_apn, ::onc::cellular_apn::kName, name);
  ret &= IsPropertyEquals(onc_apn, ::onc::cellular_apn::kUsername, username);

  const std::string* onc_password =
      onc_apn.FindString(::onc::cellular_apn::kPassword);
  if (is_password_masked) {
    ret &= policy_util::kFakeCredential == *onc_password;
  } else {
    ret &= password == *onc_password;
  }

  ret &= IsPropertyEquals(onc_apn, ::onc::cellular_apn::kAttach, attach);
  ret &= IsPropertyEquals(onc_apn, ::onc::cellular_apn::kAuthentication,
                          onc_authentication);
  if (features::IsApnRevampEnabled()) {
    const std::string* state = onc_apn.FindString(::onc::cellular_apn::kState);
    if (has_state_field) {
      ret &= state && onc_state == *state;
    } else {
      ret &= !state;
    }

    ret &= IsPropertyEquals(onc_apn, ::onc::cellular_apn::kIpType, onc_ip_type);

    const std::string* source =
        onc_apn.FindString(::onc::cellular_apn::kSource);
    if (source) {
      ret &=
          IsPropertyEquals(onc_apn, ::onc::cellular_apn::kSource, onc_source);
    }

    if (const base::Value::List* apn_types =
            onc_apn.FindList(::onc::cellular_apn::kApnTypes)) {
      if (onc_apn_types.size() != apn_types->size())
        return false;
      for (size_t i = 0; i < onc_apn_types.size(); i++)
        if (onc_apn_types[i] != ((*apn_types)[i]).GetString())
          return false;
    }
  }

  return ret;
}

}  // namespace ash::network_config
