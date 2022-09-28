// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_group_util.h"

namespace omnibox {

GroupId GroupIdForNumber(int value) {
  return GroupId_IsValid(value) ? static_cast<GroupId>(value) : GROUP_INVALID;
}

}  // namespace omnibox
