// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_USER_NOTES_FEATURES_H_
#define COMPONENTS_USER_NOTES_USER_NOTES_FEATURES_H_

#include "base/feature_list.h"

namespace user_notes {

// Feature controlling the User Notes feature on desktop platforms.
BASE_DECLARE_FEATURE(kUserNotes);

// Returns true if the User Notes feature is enabled.
bool IsUserNotesEnabled();

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_USER_NOTES_FEATURES_H_
