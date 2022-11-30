// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_REBOOT_DELETION_HELPER_H_
#define CHROME_CHROME_CLEANER_TEST_REBOOT_DELETION_HELPER_H_

#include <string>
#include <utility>
#include <vector>

#include "chrome/chrome_cleaner/os/file_path_set.h"

namespace chrome_cleaner {

// Holds the (source, destination) paths of a pending move. An empty
// destination string means deletion instead of move.
typedef std::pair<std::wstring, std::wstring> PendingMove;
typedef std::vector<PendingMove> PendingMoveVector;

// Returns true if the given file is registered to be deleted on the next
// reboot.
bool IsFileRegisteredForPostRebootRemoval(const base::FilePath& file_path);

// Unregister all of the given files from being registered to be deleted after
// the next reboot.
bool UnregisterPostRebootRemovals(const FilePathSet& paths);

// Get the list of all the pending post reboot moves.
bool GetPendingMoves(PendingMoveVector* pending_moves);

// Set all the pending moves for the nest reboot.
bool SetPendingMoves(const PendingMoveVector& pending_moves);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_REBOOT_DELETION_HELPER_H_
