// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_UTILS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_UTILS_H_

namespace tab_groups {

// Whether the local IDs are persisted, which is true for Android / iOS, but
// false in desktop.
bool AreLocalIdsPersisted();

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_UTILS_H_
