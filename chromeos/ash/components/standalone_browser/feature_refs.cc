// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/feature_refs.h"

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"

namespace ash::standalone_browser {

const std::vector<base::test::FeatureRef>& GetFeatureRefs() {
  static const base::NoDestructor<std::vector<base::test::FeatureRef>> result{
      {features::kLacrosOnly, features::kLacrosProfileMigrationForceOff}};
  return *result;
}

}  // namespace ash::standalone_browser
