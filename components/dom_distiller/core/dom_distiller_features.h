// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace dom_distiller {

// Returns true when flag enable-dom-distiller is set or reader mode is enabled
// from flags or Finch.
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES) bool IsDomDistillerEnabled();

COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
bool ShouldStartDistillabilityService();

COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeUseReadability);
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES) bool ShouldUseReadabilityDistiller();
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES) bool ShouldUseReadabilityHeuristic();
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES) int GetReadabilityHeuristicMinScore();
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
int GetReadabilityHeuristicMinContentLength();
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
int GetMinimumAllowableDistilledContentLength();

#if BUILDFLAG(IS_ANDROID)
// Feature declarations below -- alphabetical order.
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeDistillInApp);
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeImprovements);
#endif


#if BUILDFLAG(IS_IOS)
// Feature to enable the new CSS for Reader mode.
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kEnableReaderModeNewCss);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_
