// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of preferences related to Google Drive.

#ifndef COMPONENTS_DRIVE_DRIVE_PREF_NAMES_H_
#define COMPONENTS_DRIVE_DRIVE_PREF_NAMES_H_

namespace drive::prefs {

extern const char kDisableDrive[];
extern const char kDisableDriveOverCellular[];
extern const char kDriveFsEnableVerboseLogging[];
extern const char kDriveFsProfileSalt[];
extern const char kDriveFsPinnedMigrated[];
extern const char kDriveFsWasLaunchedAtLeastOnce[];
extern const char kDriveFsEnableMirrorSync[];
extern const char kDriveFsMirrorSyncMachineRootId[];
extern const char kDriveFsBulkPinningEnabled[];

}  // namespace drive::prefs

#endif  // COMPONENTS_DRIVE_DRIVE_PREF_NAMES_H_
