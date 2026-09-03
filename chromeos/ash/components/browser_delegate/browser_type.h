// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BROWSER_DELEGATE_BROWSER_TYPE_H_
#define CHROMEOS_ASH_COMPONENTS_BROWSER_DELEGATE_BROWSER_TYPE_H_

namespace ash {

// Enumerates the various types of browser.
// See BrowserController and BrowserDelegate.
// TODO(crbug.com/369689187): Replace kOther once we know what we need.
enum class BrowserType {
  kApp,
  kAppPopup,
  kDevTools,
  kNormal,
  kPopup,
  kOther,
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BROWSER_DELEGATE_BROWSER_TYPE_H_
