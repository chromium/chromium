// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_

namespace web_app {

// Reflects
// https://source.chromium.org/chromium/chromium/src/+/main:components/sync/protocol/web_app_specifics.proto;l=44;drc=0758d11f3d4db21e8e8b1b3a5600261fde150afb
enum class UserDisplayMode {
  // The user prefers that the web app open in a browser context, like a tab.
  kBrowser,

  // The user prefers the web app open in a standalone window experience.
  kStandalone,

  // The user wants the web app to open in a standalone window experience with
  // tabs.
  kTabbed,
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_USER_DISPLAY_MODE_H_
