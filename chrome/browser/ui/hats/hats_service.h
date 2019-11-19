// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class Profile;

// This class provides the client side logic for determining if a
// survey should be shown for any trigger based on input from a finch
// configuration. It is created on a per profile basis.
class HatsService : public KeyedService {
 public:
  struct SurveyMetadata {
    SurveyMetadata();
    ~SurveyMetadata();

    base::Optional<int> last_major_version;
    base::Optional<base::Time> last_survey_started_time;
  };

  explicit HatsService(Profile* profile);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // This is the public function that will launch the "satisfaction" survey if
  // it's appropriate.
  void LaunchSatisfactionSurvey();

  // Returns the en-us site ID for the HaTS survey.
  const std::string& en_site_id() const { return en_site_id_; }

  void SetSurveyMetadataForTesting(const SurveyMetadata& metadata);

 private:
  // This returns true is the survey trigger specified should be shown.
  bool ShouldShowSurvey(const std::string& trigger) const;

  // Profile associated with this service.
  Profile* const profile_;

  // Trigger string identifier.
  const std::string trigger_;

  // Percent of users [0,1] that will see the survey.
  const double probability_;

  // Site ID for the survey.
  const std::string en_site_id_;

  DISALLOW_COPY_AND_ASSIGN(HatsService);
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
