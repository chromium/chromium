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

#if BUILDFLAG(IS_ANDROID)
using LocalTabID = int;
using LocalTabGroupID = int;
#else
using LocalTabID = base::Token;
using LocalTabGroupID = tab_groups::TabGroupId;
#endif

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TYPES_H_
