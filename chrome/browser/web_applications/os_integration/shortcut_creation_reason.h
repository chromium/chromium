// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_SHORTCUT_CREATION_REASON_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_SHORTCUT_CREATION_REASON_H_

namespace web_app {

// This encodes the cause of shortcut creation as the correct behavior in each
// case is implementation specific.
enum ShortcutCreationReason {
  SHORTCUT_CREATION_BY_USER,
  SHORTCUT_CREATION_AUTOMATED,
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_SHORTCUT_CREATION_REASON_H_
