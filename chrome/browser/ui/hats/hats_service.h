// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_

#include <stddef.h>

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

struct HatsFinchConfig {
  HatsFinchConfig();
  ~HatsFinchConfig();
  HatsFinchConfig(const HatsFinchConfig& other);

  double probability;   // This is the percent of users [0,1] that will see the
                        // survey
  std::string trigger;  // This is the name of the survey in question.

  // This is a map between the locale being presented and the site ID used to
  // fetch the survey.
  std::map<std::string, std::string> site_ids;
};

// This class provides the client side logic for determining if a
// survey should be shown for any trigger based on input from a finch
// configuration. It is created on a per profile basis.
class HatsService : public KeyedService {
 public:
  explicit HatsService(Profile* profile);

  // This is the public function that will launch the "satisfaction" survey if
  // it's appropriate.
  void LaunchSatisfactionSurvey();

 private:
  // This returns true is the survey trigger specified should be shown.
  bool ShouldShowSurvey(const std::string& trigger) const;

  // a temporary flag to ensure that hats is not launched multiple times
  // TODO: replace with pref lookup
  static bool launch_hats_;
  Profile* profile_;
  const HatsFinchConfig hats_finch_config_;

  FRIEND_TEST_ALL_PREFIXES(HatsForceEnabledTest, ParamsWithAForcedFlagTest);

  DISALLOW_COPY_AND_ASSIGN(HatsService);
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
