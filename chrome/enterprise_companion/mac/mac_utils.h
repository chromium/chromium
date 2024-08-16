// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_MAC_MAC_UTILS_H_
#define CHROME_ENTERPRISE_COMPANION_MAC_MAC_UTILS_H_

#include <sys/types.h>

#include <optional>

namespace enterprise_companion {

// Queries the System Configuration dynamic store for console users. This is a
// more reliable way to determine the the UID of a logged-in user than the
// traditional stat of /dev/console, which can be owned by root during Chrome
// Remote Desktop sessions.
std::optional<uid_t> GuessLoggedInUser();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_MAC_MAC_UTILS_H_
