// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_STRING_UTILS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_STRING_UTILS_H_

#include <string>

namespace base {
class TimeDelta;
}  // namespace base

namespace tab_groups {

// Returns a user-friendly localized string representing the duration since the
// tab group creation date. E.g.:
// - "Created just now" when under a minute.
// - "Created 1 minute ago".
// - "Created 37 minute ago".
// - "Created 2 hours ago".
// - "Created 4 days ago".
// - "Created 1 month ago".
// - "Created 2 years ago".
std::u16string LocalizedElapsedTimeSinceCreation(
    base::TimeDelta elapsed_time_since_creation);

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_STRING_UTILS_H_
