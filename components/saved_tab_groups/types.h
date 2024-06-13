// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TYPES_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TYPES_H_

#include "base/logging.h"
#include "base/token.h"
#include "build/build_config.h"
#include "components/tab_groups/tab_group_id.h"

namespace tab_groups {

// Types for tab and tab group IDs.
#if BUILDFLAG(IS_ANDROID)
using LocalTabID = int;
using LocalTabGroupID = base::Token;
#else
using LocalTabID = base::Token;
using LocalTabGroupID = tab_groups::TabGroupId;
#endif

// Base context for tab group actions. Platforms can subclass this to pass
// additional context such as a browser window.
struct TabGroupActionContext {};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TYPES_H_
