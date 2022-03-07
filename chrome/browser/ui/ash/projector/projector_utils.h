// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_UTILS_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_UTILS_H_

class Profile;

namespace drive {
class DriveIntegrationService;
}

// Returns whether Projector is allowed for given `profile`.
bool IsProjectorAllowedForProfile(const Profile* profile);

drive::DriveIntegrationService* GetDriveIntegrationServiceForActiveProfile();

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_UTILS_H_
