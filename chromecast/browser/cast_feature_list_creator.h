// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_FEATURE_LIST_CREATOR_H_
#define CHROMECAST_BROWSER_CAST_FEATURE_LIST_CREATOR_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "chromecast/base/process_types.h"

class PrefService;

namespace chromecast {

// Creator for the singleton |FeatureList|. Stateful due to having to create and
// hold a |PrefService| instance until |CastBrowserProcess| takes ownership when
// the full browser process starts,
class CastFeatureListCreator {
 public:
  CastFeatureListCreator();
  CastFeatureListCreator(const CastFeatureListCreator&) = delete;
  CastFeatureListCreator& operator=(const CastFeatureListCreator&) = delete;
  virtual ~CastFeatureListCreator();

  // Creates the |PrefService| and uses it to initialize |FeatureList|. Retains
  // ownership of the |PrefService|. Based on the given |ProcessType|, different
  // files will be used to store prefs.
  void CreatePrefServiceAndFeatureList(
      ProcessType process_type = ProcessType::kCastService);

  // Takes ownership of the |PrefService| previously created.
  std::unique_ptr<PrefService> TakePrefService();

  // Sets the extra features to be enabled.
  void SetExtraEnableFeatures(
      const std::vector<const base::Feature*>& extra_enable_features);

  // Sets the extra features to be disabled.
  void SetExtraDisableFeatures(
      const std::vector<const base::Feature*>& extra_disable_features);

 private:
  // Holds the |PrefService| until TakePrefService() is called and ownership
  // is taken away.
  std::unique_ptr<PrefService> pref_service_;
  // Extra features that can be enabled at run time.
  std::string extra_enable_features_;
  // Extra features that can be disabled at run time.
  std::string extra_disable_features_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_FEATURE_LIST_CREATOR_H_
