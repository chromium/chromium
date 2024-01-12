// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_LINUX_UTIL_H_
#define CHROME_UPDATER_UTIL_LINUX_UTIL_H_

namespace updater {

// Filename of the non side-by-side launcher. The file is a hardlink to the
// qualified version of the updater.
extern const char kLauncherName[];

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_LINUX_UTIL_H_
