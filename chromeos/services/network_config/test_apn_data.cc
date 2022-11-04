// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/test_apn_data.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos::network_config {
namespace {
// TODO(b/162365553) Remove when shill constants are added.
constexpr char kShillApnId[] = "id";
constexpr char kShillApnAuthenticationType[] = "authentication_type";
constexpr char kShillApnTypes[] = "apn_types";
}  //  namespace

TestApnData::TestApnData()
    : mojo_state(mojom::ApnState::kEnabled),
      onc_state(::onc::cellular_apn::kStateEnabled),
      mojo_authentication_type(mojom::ApnAuthenticationType::kAutomatic),
      onc_authentication_type(
          ::onc::cellular_apn::kAuthenticationTypeAutomatic),
      mojo_ip_type(mojom::ApnIpType::kAutomatic),
      onc_ip_type(::onc::cellular_apn::kIpTypeAutomatic) {}

TestApnData::TestApnData(std::string access_point_name,
                         std::string name,
                         std::string username,
                         std::string password,
                         std::string attach,
                         std::string id,
                         mojom::ApnState mojo_state,
                         std::string onc_state,
                         mojom::ApnAuthenticationType mojo_authentication_type,
                         std::string onc_authentication_type,
                         mojom::ApnIpType mojo_ip_type,
                         std::string onc_ip_type,
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
      mojo_authentication_type(mojo_authentication_type),
      onc_authentication_type(onc_authentication_type),
      mojo_ip_type(mojo_ip_type),
      onc_ip_type(onc_ip_type),
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
  if (ash::features::IsApnRevampEnabled()) {
    apn->id = id;
    apn->authentication_type = mojo_authentication_type;
    apn->ip_type = mojo_ip_type;
    apn->apn_types = mojo_apn_types;
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
  if (ash::features::IsApnRevampEnabled()) {
    apn.Set(::onc::cellular_apn::kId, id);
    apn.Set(::onc::cellular_apn::kState, onc_state);
    apn.Set(::onc::cellular_apn::kAuthenticationType, onc_authentication_type);
    apn.Set(::onc::cellular_apn::kIpType, onc_ip_type);

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
  if (ash::features::IsApnRevampEnabled()) {
    apn.Set(kShillApnId, id);
    apn.Set(kShillApnAuthenticationType, onc_authentication_type);
    apn.Set(shill::kApnIpTypeProperty, onc_ip_type);

    base::Value::List apn_types;
    for (const std::string& apn_type : onc_apn_types)
      apn_types.Append(apn_type);
    apn.Set(kShillApnTypes, std::move(apn_types));
  }
  return apn;
}

std::string TestApnData::AsApnShillDict() const {
  // This will serialize the dictionary into valid JSON
  return AsShillApn().DebugString();
}

bool TestApnData::IsMojoApnEquals(const mojom::ApnProperties& apn) const {
  bool ret = access_point_name == apn.access_point_name;

  static auto MatchOptionalString =
      [](const std::string& expected,
         const absl::optional<std::string>& actual) -> bool {
    if (actual.has_value())
      return expected == *actual;
    return expected.empty();
  };
  ret &= MatchOptionalString(name, apn.name);
  ret &= MatchOptionalString(username, apn.username);
  ret &= MatchOptionalString(password, apn.password);
  ret &= MatchOptionalString(attach, apn.attach);

  if (ash::features::IsApnRevampEnabled()) {
    ret &= mojo_authentication_type == apn.authentication_type;
    ret &= mojo_ip_type == apn.ip_type;
    ret &= mojo_apn_types == apn.apn_types;
  }
  return ret;
}

}  // namespace chromeos::network_config
