// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/values.h"
#include "dbus/object_path.h"

namespace ash {

// Metadata representing an eSIM profile.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimProfile {
 public:
  // Note that numerical values are stored in prefs and should not be changed or
  // reused.
  enum class State {
    // Profile is not installed on the device. This likely means that it was
    // discovered via SMDS.
    kPending = 0,

    // Profile is being installed (i.e., being loaded into the EIUCC).
    kInstalling = 1,

    // Profile is installed but inactive.
    kInactive = 2,

    // Profile is installed and active.
    kActive = 3,
  };

  // Returns null if the provided value does not have the required dictionary
  // properties. Should be provided a dictionary created via
  // ToDictionaryValue().
  static std::optional<CellularESimProfile> FromDictionaryValue(
      const base::Value::Dict& value);

  CellularESimProfile(State state,
                      const dbus::ObjectPath& path,
                      const std::string& eid,
                      const std::string& iccid,
                      const std::u16string& name,
                      const std::u16string& nickname,
                      const std::u16string& service_provider,
                      const std::string& activation_code);
  CellularESimProfile(const CellularESimProfile&);
  CellularESimProfile& operator=(const CellularESimProfile&);
  ~CellularESimProfile();

  State state() const { return state_; }
  const dbus::ObjectPath& path() const { return path_; }
  const std::string& eid() const { return eid_; }
  const std::string& iccid() const { return iccid_; }
  const std::u16string& name() const { return name_; }
  const std::u16string& nickname() const { return nickname_; }
  const std::u16string& service_provider() const { return service_provider_; }
  const std::string& activation_code() const { return activation_code_; }

  base::Value::Dict ToDictionaryValue() const;

  bool operator==(const CellularESimProfile& other) const;
  bool operator!=(const CellularESimProfile& other) const;

 private:
  State state_;

  // Dbus path to the Hermes eSIM profile object.
  dbus::ObjectPath path_;

  // EID of the Euicc in which this profile is installed or available for
  // installation.
  std::string eid_;

  // A 19-20 character long numeric id that uniquely identifies this profile.
  std::string iccid_;

  // Service provider’s name for this profile. e.g. “Verizon Plan 1”
  std::u16string name_;

  // A user configurable nickname for this profile. e.g. “My Personal SIM”
  std::u16string nickname_;

  // Name of the service provider. e.g. “Verizon Wireless”
  std::u16string service_provider_;

  // An alphanumeric code that can be used to install this profile.
  std::string activation_code_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_PROFILE_H_
