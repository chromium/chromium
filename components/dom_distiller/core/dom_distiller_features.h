// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_

#include <optional>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace dom_distiller {

// Returns true when flag enable-dom-distiller is set or reader mode is enabled
// from flags or Finch.
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES) bool IsDomDistillerEnabled();

COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
bool ShouldStartDistillabilityService();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
inline constexpr int kReadabilityHeuristicMinScore = 50;
inline constexpr int kReadabilityHeuristicMinContentLength = 160;
#else
inline constexpr int kReadabilityHeuristicMinScore = 100;
inline constexpr int kReadabilityHeuristicMinContentLength = 200;
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeSupportNewFonts);
#endif

#if BUILDFLAG(IS_ANDROID)
// Feature declarations below -- alphabetical order.
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeBlurTransitionAnimation);
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeDelayBottomSheetPeek);
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeDistillInApp);
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeDelayBottomSheetPeek);
COMPONENT_EXPORT(DOM_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeToggleLinks);
#endif

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_
