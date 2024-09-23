// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_CELLULAR_UTIL_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_CELLULAR_UTIL_H_

#include <optional>
#include <string>

#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "ui/base/interaction/state_observer.h"

namespace ash {

// Helper class to simplify the definition of eUICC constants.
class EuiccInfo {
 public:
  // The `id` parameter is used when generating values for each of the
  // different eUICC properties below.
  explicit EuiccInfo(unsigned int id);
  ~EuiccInfo();

  const std::string& path() const { return path_; }
  const std::string& eid() const { return eid_; }

 private:
  const std::string path_;
  const std::string eid_;
};

// Helper class to simplify the definition of SIM profile constants.
class SimInfo {
 public:
  // The `id` parameter is used when generating values for each of the different
  // SIM properties below.
  explicit SimInfo(unsigned int id);
  ~SimInfo();

  // Connects to the cellular network corresponding to the eSIM profile matching
  // the properties of this class.
  void Connect() const;

  // Disconnects from the cellular network corresponding to the eSIM profile
  // matching the properties of this class.
  void Disconnect() const;

  const std::string& guid() const { return guid_; }
  const std::string& profile_path() const { return profile_path_; }
  const std::string& iccid() const { return iccid_; }
  const std::string& name() const { return name_; }
  const std::string& nickname() const { return nickname_; }
  const std::string& service_provider() const { return service_provider_; }
  const std::string& service_path() const { return service_path_; }
  const std::string& activation_code() const { return activation_code_; }

 private:
  const std::string guid_;
  const std::string profile_path_;
  const std::string iccid_;
  const std::string name_;
  const std::string nickname_;
  const std::string service_provider_;
  const std::string service_path_;
  std::string activation_code_;
};

// Helper function to configure an eSIM profile and corresponding Shill service.
void ConfigureEsimProfile(const EuiccInfo& euicc_info,
                          const SimInfo& esim_info,
                          bool connected);

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_CELLULAR_UTIL_H_
