// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_

#include "base/feature_list.h"

namespace dom_distiller {

extern const base::Feature kReaderMode;

// Returns true when flag enable-dom-distiller is set or enabled from Finch.
bool IsDomDistillerEnabled();

bool ShouldStartDistillabilityService();

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_FEATURES_H_
