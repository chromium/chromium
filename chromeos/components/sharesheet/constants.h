// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SHARESHEET_CONSTANTS_H_
#define CHROMEOS_COMPONENTS_SHARESHEET_CONSTANTS_H_

namespace sharesheet {

// The source from which the sharesheet was launched from.
// This enum is for recording histograms and must be treated as append-only.
enum class LaunchSource {
  kUnknown = 0,
  kFilesAppShareButton = 1,
  kFilesAppContextMenu = 2,
  kWebShare = 3,
  kArcNearbyShare = 4,
  kOmniboxShare = 5,
  kMaxValue = kOmniboxShare,
};

enum class SharesheetResult {
  kSuccess,            // Successfully passed data to selected target.
  kCancel,             // Share was cancelled before completion.
  kErrorAlreadyOpen,   // Share failed because the sharesheet is already open.
  kErrorWindowClosed,  // Parent window closed before sharesheet could be shown.
};

}  // namespace sharesheet

#endif  // CHROMEOS_COMPONENTS_SHARESHEET_CONSTANTS_H_
