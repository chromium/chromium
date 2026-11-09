// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_FEATURES_H_
#define COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_FEATURES_H_

#include <optional>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace web_distiller {

// Returns true when flag enable-dom-distiller is set or reader mode is enabled
// from flags or Finch.
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES) bool IsWebDistillerEnabled();

COMPONENT_EXPORT(WEB_DISTILLER_FEATURES)
bool ShouldStartDistillabilityService();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
inline constexpr int kHeuristicMinScore = 50;
inline constexpr int kHeuristicMinContentLength = 160;
#else
inline constexpr int kHeuristicMinScore = 100;
inline constexpr int kHeuristicMinContentLength = 200;
#endif

COMPONENT_EXPORT(WEB_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kSyncReaderForAccessibility);

#if BUILDFLAG(IS_ANDROID)
// Feature declarations below -- alphabetical order.
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeBlurTransitionAnimation);
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeDelayBottomSheetPeek);
COMPONENT_EXPORT(WEB_DISTILLER_FEATURES)
BASE_DECLARE_FEATURE(kReaderModeToggleLinks);
#endif

}  // namespace web_distiller

#endif  // COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_FEATURES_H_
