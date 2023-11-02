// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_DEFINITIONS_H_
#define COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_DEFINITIONS_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/assist_ranker/predictor_config.h"

namespace assist_ranker {

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kContextualSearchRankerQuery);
const PredictorConfig GetContextualSearchPredictorConfig();
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_DEFINITIONS_H_
