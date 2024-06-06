// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_UTIL_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_UTIL_H_

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

// Helper class to simplify the definition of eSIM profile constants.
class EsimInfo {
 public:
  // The `id` parameter is used when generating values for each of the different
  // eSIM properties below.
  explicit EsimInfo(unsigned int id);
  ~EsimInfo();

  const std::string& profile_path() const { return profile_path_; }
  const std::string& iccid() const { return iccid_; }
  const std::string& name() const { return name_; }
  const std::string& nickname() const { return nickname_; }
  const std::string& service_provider() const { return service_provider_; }
  const std::string& service_path() const { return service_path_; }

 private:
  const std::string profile_path_;
  const std::string iccid_;
  const std::string name_;
  const std::string nickname_;
  const std::string service_provider_;
  const std::string service_path_;
};

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_ESIM_UTIL_H_
