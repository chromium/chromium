// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_SWITCHES_H_
#define COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_SWITCHES_H_

#include "base/command_line.h"
#include "base/component_export.h"

namespace switches {

// Switch to enable the distillability service on the renderer.
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES)
extern const char kEnableDistillabilityService[];

// Switch to enable the web distiller.
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES)
extern const char kEnableWebDistiller[];

// Switch to enable specific heuristics for detecting if a page is distillable
// or not.
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES)
extern const char kReaderModeHeuristics[];

// Switch to control the display of the distiller feedback form.
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES)
extern const char kReaderModeFeedback[];

namespace reader_mode_heuristics {
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES) extern const char kAdaBoost[];
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES) extern const char kAllArticles[];
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES) extern const char kOGArticle[];
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES) extern const char kAlwaysTrue[];
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES) extern const char kNone[];
}  // namespace reader_mode_heuristics

COMPONENT_EXPORT(WEB_DISTILLER_FEATURES)
extern const char kReaderModeDiscoverabilityParamName[];
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES)
extern const char kReaderModeOfferInSettings[];

}  // namespace switches

#endif  // COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_SWITCHES_H_
