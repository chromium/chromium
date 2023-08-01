// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SCREENTIME_FAKE_WEBPAGE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_SCREENTIME_FAKE_WEBPAGE_CONTROLLER_H_

#include <vector>

#include "chrome/browser/ui/cocoa/screentime/webpage_controller.h"

namespace screentime {

// An implementation of WebpageController that is not backed by the real
// ScreenTime framework. This is used for testing and development on pre-11.0
// devices that don't have the real ScreenTime API available.
//
// FakeWebpageController implements the following behavior:
// 1. The ScreenTime "shield" view is a flat blue layer
// 2. Every navigation causes it to toggle blocking / not blocking state
//
// Further testing hooks may be added to this class in future.
class FakeWebpageController : public WebpageController {
 public:
  FakeWebpageController(const BlockedChangedCallback& callback);
  ~FakeWebpageController() override;

  NSView* GetView() override;
  void PageURLChangedTo(const GURL& url) override;

  const std::vector<GURL>& visited_urls_for_testing() const {
    return visited_urls_;
  }

 private:
  bool enabled_ = false;
  NSView* __strong view_;
  BlockedChangedCallback blocked_changed_callback_;

  // For unit tests:
  std::vector<GURL> visited_urls_;
};

}  // namespace screentime

#endif  // CHROME_BROWSER_UI_COCOA_SCREENTIME_FAKE_WEBPAGE_CONTROLLER_H_
