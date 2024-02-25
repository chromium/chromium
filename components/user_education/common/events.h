// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_EVENTS_H_
#define COMPONENTS_USER_EDUCATION_COMMON_EVENTS_H_

#include "base/component_export.h"
#include "ui/base/interaction/element_tracker.h"

namespace user_education {

DECLARE_EXPORTED_CUSTOM_ELEMENT_EVENT_TYPE(
    COMPONENT_EXPORT(USER_EDUCATION_COMMON_EVENTS),
    kHelpBubbleAnchorBoundsChangedEvent);

DECLARE_EXPORTED_CUSTOM_ELEMENT_EVENT_TYPE(
    COMPONENT_EXPORT(USER_EDUCATION_COMMON_EVENTS),
    kHelpBubbleNextButtonClickedEvent);

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_EVENTS_H_
