// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_UTILS_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_UTILS_H_

#include <vector>

namespace base {
class FilePath;
}  // namespace base

class Profile;

// Returns whether Projector is allowed for given `profile`.
bool IsProjectorAllowedForProfile(const Profile* profile);

// Returns whether the Projector app is enabled.
bool IsProjectorAppEnabled(const Profile* profile);

// Launches the Projector SWA with the specified files. If the app is already
// open, then reuse the existing window.
void LaunchProjectorAppWithFiles(std::vector<base::FilePath> files);

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_UTILS_H_
