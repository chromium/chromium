// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/with_crosapi_param.h"

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/common/chrome_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {
namespace test {

WithCrosapiParam::WithCrosapiParam() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (GetParam() == CrosapiParam::kEnabled) {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kLacrosSupport, features::kWebAppsCrosapi}, {});
  } else {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kWebAppsCrosapi, ash::features::kLacrosPrimary});
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

WithCrosapiParam::~WithCrosapiParam() = default;

// static
std::string WithCrosapiParam::ParamToString(
    testing::TestParamInfo<CrosapiParam> param) {
  switch (param.param) {
    case CrosapiParam::kDisabled:
      return "WebAppsCrosapiDisabled";
    case CrosapiParam::kEnabled:
      return "WebAppsCrosapiEnabled";
  }
}

}  // namespace test
}  // namespace web_app
