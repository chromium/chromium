// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_CONTENT_RECORD_PASSWORD_STATE_H_
#define COMPONENTS_SESSIONS_CONTENT_CONTENT_RECORD_PASSWORD_STATE_H_

#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/sessions_export.h"

namespace content {
class NavigationEntry;
}

namespace sessions {

// Helper functions for storing/getting PasswordState in a NavigationEntry.
SESSIONS_EXPORT SerializedNavigationEntry::PasswordState
GetPasswordStateFromNavigation(content::NavigationEntry* entry);

SESSIONS_EXPORT void SetPasswordStateInNavigation(
    SerializedNavigationEntry::PasswordState state,
    content::NavigationEntry* entry);

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_CONTENT_RECORD_PASSWORD_STATE_H_
