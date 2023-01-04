// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_SAVE_DESKTOP_SNAPSHOT_H_
#define CHROME_TEST_BASE_SAVE_DESKTOP_SNAPSHOT_H_

#include "base/files/file_path.h"

// A command line switch to specify the output directory into which snapshots
// are to be saved; e.g., in case an always-on-top window is found.
extern const char kSnapshotOutputDir[];

// Saves a snapshot of the desktop to a file in kSnapshotOutputDir or
// |output_dir|, returning the path to the file if created. An empty path is
// returned if no new snapshot is created.
base::FilePath SaveDesktopSnapshot();
base::FilePath SaveDesktopSnapshot(const base::FilePath& output_dir);

#endif  // CHROME_TEST_BASE_SAVE_DESKTOP_SNAPSHOT_H_
