// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SWITCHES_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SWITCHES_H_

#include "base/command_line.h"

namespace switches {

// Switch to enable the distillability service on the renderer.
extern const char kEnableDistillabilityService[];

// Switch to enable the DOM distiller.
extern const char kEnableDomDistiller[];

// Switch to enable specific heuristics for detecting if a page is distillable
// or not.
extern const char kReaderModeHeuristics[];

// Switch to control the display of the distiller feedback form.
extern const char kReaderModeFeedback[];

namespace reader_mode_heuristics {
extern const char kAdaBoost[];
extern const char kAllArticles[];
extern const char kOGArticle[];
extern const char kAlwaysTrue[];
extern const char kNone[];
}  // namespace reader_mode_heuristics

extern const char kReaderModeDiscoverabilityParamName[];
extern const char kReaderModeOfferInSettings[];

}  // namespace switches

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SWITCHES_H_
