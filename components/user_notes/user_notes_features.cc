// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/user_notes_features.h"

namespace user_notes {

BASE_FEATURE(kUserNotes, "UserNotes", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUserNotesEnabled() {
  return base::FeatureList::IsEnabled(kUserNotes);
}

}  // namespace user_notes
