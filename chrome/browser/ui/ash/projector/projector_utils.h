// Copyright 2022 The Chromium Authors
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

// Returns whether a file is a media (webm) file.
bool IsMediaFile(const base::FilePath& path);

// Returns whether a file is a screencast metadata file,
// ".projector" for v1 screencast, ".screencast" for v2
bool IsMetadataFile(const base::FilePath& path);

// Sends the specified `files` to the Projector SWA. The app must be already
// open as a prerequisite before calling this function.
// Note: this function passes by value to avoid a copy in the implementation.
// Transfer ownership using std::move() if you want to avoid a copy when calling
// this function.
void SendFilesToProjectorApp(std::vector<base::FilePath> files);

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_UTILS_H_
