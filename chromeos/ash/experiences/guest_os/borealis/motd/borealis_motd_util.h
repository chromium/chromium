// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_UTIL_H_
#define CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_UTIL_H_

namespace borealis {

// Gets the current milestone from the version information, to be used
// when requesting the MOTD message for the Borealis MOTD dialog
int GetMilestone();

}  // namespace borealis

#endif  // CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_UTIL_H_
