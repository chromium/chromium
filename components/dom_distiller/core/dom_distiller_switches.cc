// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_switches.h"

namespace switches {

const char kEnableDistillabilityService[] = "enable-distillability-service";
const char kEnableDomDistiller[] = "enable-dom-distiller";
const char kReaderModeHeuristics[] = "reader-mode-heuristics";
const char kReaderModeFeedback[] = "reader-mode-feedback";

namespace reader_mode_heuristics {
const char kAdaBoost[] = "adaboost";
const char kAllArticles[] = "allarticles";
const char kOGArticle[] = "opengraph";
const char kAlwaysTrue[] = "alwaystrue";
const char kNone[] = "none";
}  // namespace reader_mode_heuristics

const char kReaderModeDiscoverabilityParamName[] = "discoverability";
const char kReaderModeOfferInSettings[] = "offer-in-settings";

}  // namespace switches
