// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use the <code>chrome.enterprise.login</code> API to exit Managed Guest
// sessions. Note: This API is only available to extensions installed by
// enterprise policy in ChromeOS Managed Guest sessions.
[platforms = ("chromeos"),
 implemented_in = "chrome/browser/extensions/api/enterprise_login/enterprise_login_api.h"]
interface Login {
  // Exits the current managed guest session.
  static Promise<undefined> exitCurrentManagedGuestSession();
};

partial interface Enterprise {
  static attribute Login login;
};

partial interface Browser {
  static attribute Enterprise enterprise;
};
