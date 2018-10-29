// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATER_STATE_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATER_STATE_H_

#include <map>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/version.h"

namespace update_client {

class UpdaterState {
 public:
  using Attributes = std::map<std::string, std::string>;

  static const char kIsEnterpriseManaged[];

  // Returns a map of items representing the state of an updater.
  // If |is_machine| is true, this indicates that the updater state corresponds
  // to the machine instance of the updater. Returns nullptr on
  // the platforms and builds where this feature is not supported.
  static std::unique_ptr<Attributes> GetState(bool is_machine);

  ~UpdaterState();

 private:
  FRIEND_TEST_ALL_PREFIXES(UpdaterStateTest, Serialize);

  explicit UpdaterState(bool is_machine);

  // This function is best-effort. It updates the class members with
  // the relevant values that could be retrieved.
  void ReadState();

  // Builds the map of state attributes by serializing this object state.
  Attributes BuildAttributes() const;

  static std::string GetUpdaterName();
  static base::Version GetUpdaterVersion(bool is_machine);
  static bool IsAutoupdateCheckEnabled();
  static bool IsEnterpriseManaged();
  static base::Time GetUpdaterLastStartedAU(bool is_machine);
  static base::Time GetUpdaterLastChecked(bool is_machine);

  static int GetUpdatePolicy();

  static std::string NormalizeTimeDelta(const base::TimeDelta& delta);

  // True if the Omaha updater is installed per-machine.
  // The MacOS implementation ignores the value of this member but this may
  // change in the future.
  bool is_machine_;
  std::string updater_name_;
  base::Version updater_version_;
  base::Time last_autoupdate_started_;
  base::Time last_checked_;
  bool is_enterprise_managed_ = false;
  bool is_autoupdate_check_enabled_ = false;
  int update_policy_ = 0;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATER_STATE_H_
