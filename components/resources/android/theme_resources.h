// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RESOURCES_ANDROID_THEME_RESOURCES_H_
#define COMPONENTS_RESOURCES_ANDROID_THEME_RESOURCES_H_

// LINK_RESOURCE_ID will use an ID defined by grit, so no-op.
#define LINK_RESOURCE_ID(c_id, java_id)
// For DECLARE_RESOURCE_ID, make an entry in an enum.
#define DECLARE_RESOURCE_ID(c_id, java_id) c_id,

enum {
  // Not used; just provides a starting value for the enum. These must
  // not conflict with IDR_* values, which top out at 2^16 - 1.
  ANDROID_COMPONENTS_RESOURCE_ID_NONE = 1 << 16,
#include "components/resources/android/autofill_resource_id.h"
#include "components/resources/android/blocked_content_resource_id.h"
#include "components/resources/android/page_info_resource_id.h"
#include "components/resources/android/permissions_resource_id.h"
#include "components/resources/android/sms_resource_id.h"
#include "components/resources/android/webxr_resource_id.h"
  ANDROID_COMPONENTS_RESOURCE_ID_MAX,
};

#undef LINK_RESOURCE_ID
#undef DECLARE_RESOURCE_ID

#endif  // COMPONENTS_RESOURCES_ANDROID_THEME_RESOURCES_H_
