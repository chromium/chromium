// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_TYPES_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_TYPES_H_

namespace chrome {
namespace startup {

enum class IsProcessStartup : bool {
  kNo,  // Session is being created when a Chrome process is already running,
        // e.g. clicking on a taskbar icon when Chrome is already running, or
        // restoring a profile.
  kYes  // Session is being created when the Chrome process is not already
        // running.
};

enum class IsFirstRun : bool {
  kNo,  // Session is being created after Chrome has already been run at least
        // once on the system.
  kYes  // Session is being created immediately after Chrome has been installed
        // on the system.
};

}  // namespace startup
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_TYPES_H_
