// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/test_util.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"

namespace tabs {

namespace {

class TabFeaturesFake : public tabs::TabFeatures {
 public:
  void Init(tabs::TabInterface& tab, Profile* profile) override {}
};

}  // namespace

PreventTabFeatureInitialization::PreventTabFeatureInitialization() {
  tabs::TabFeatures::ReplaceTabFeaturesForTesting(
      base::BindRepeating([]() -> std::unique_ptr<tabs::TabFeatures> {
        return std::make_unique<TabFeaturesFake>();
      }));
}

PreventTabFeatureInitialization::~PreventTabFeatureInitialization() {
  // Pass in a null callback to disable the functionality.
  tabs::TabFeatures::ReplaceTabFeaturesForTesting(base::NullCallback());
}

}  // namespace tabs
