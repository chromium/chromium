// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pref_names.h"

namespace ash::settings::prefs {

// Boolean that indicates that ash is restarting after the completion of
// "Sanitize Settings", and the post-sanitize UI should be displayed after
// restart. Once the post-sanitize UI is displayed, this value will be reset.
const char kSanitizeCompleted[] = "chromeos.sanitize_completed";

}  // namespace ash::settings::prefs
