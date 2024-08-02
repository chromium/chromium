// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_FEATURES_H_
#define COMPONENTS_INPUT_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace input::features {

#if BUILDFLAG(IS_ANDROID)
// If enabled, touch input on Android is handled on Viz process. The feature is
// still in development and might not have any functional effects yet.
// Design doc for InputVizard project for moving touch input to viz on Android:
// https://docs.google.com/document/d/1mcydbkgFCO_TT9NuFE962L8PLJWT2XOfXUAPO88VuKE

COMPONENT_EXPORT(INPUT) BASE_DECLARE_FEATURE(kInputOnViz);
#endif

}  // namespace input::features

#endif  // COMPONENTS_INPUT_FEATURES_H_
