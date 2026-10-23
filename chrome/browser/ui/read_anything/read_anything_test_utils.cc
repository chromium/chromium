// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_test_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "components/prefs/pref_service.h"

namespace read_anything::test {

void PreferSidePanel(Profile* profile) {
  profile->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingLastOpenedPresentationState,
      static_cast<int>(mojom::ReadAnythingPresentationState::kInSidePanel));
}

}  // namespace read_anything::test
