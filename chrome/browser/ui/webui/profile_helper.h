// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PROFILE_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_PROFILE_HELPER_H_

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"

namespace webui {

// Opens a new window for |profile|, or:
// - if the profile is locked, opens the user manager instead
// - if the profile picker is already open, focuses it instead
// Exposed for testing.
void OpenNewWindowForProfile(Profile* profile);

// Deletes the profile at the given |file_path|.
void DeleteProfileAtPath(base::FilePath file_path,
                         ProfileMetrics::ProfileDelete deletion_source);

}  // namespace webui



#endif  // CHROME_BROWSER_UI_WEBUI_PROFILE_HELPER_H_
