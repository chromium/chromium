// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_NET_BROWSER_URL_OPENER_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_NET_BROWSER_URL_OPENER_H_

#include "url/gurl.h"

namespace arc {

// Delegate class to open URLs through Chrome browser.
class BrowserUrlOpener {
 public:
  BrowserUrlOpener();
  BrowserUrlOpener(const BrowserUrlOpener&) = delete;
  BrowserUrlOpener& operator=(const BrowserUrlOpener&) = delete;
  virtual ~BrowserUrlOpener();

  // Get the singleton instance of this class.
  static BrowserUrlOpener* Get();

  // Opens the specified `url` in a new browser tab.
  virtual void OpenUrl(GURL url) = 0;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_NET_BROWSER_URL_OPENER_H_
