// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SCREENTIME_WEBPAGE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_COCOA_SCREENTIME_WEBPAGE_CONTROLLER_IMPL_H_

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/screentime/webpage_controller.h"

@class BlockedObserver;
@class STWebpageController;

namespace screentime {

// This class wraps the STWebpageController screentime class, to allow for tests
// to use a fake controller.
class WebpageControllerImpl : public WebpageController {
 public:
  WebpageControllerImpl(const BlockedChangedCallback& callback);
  ~WebpageControllerImpl() override;

  NSView* GetView() override;
  void PageURLChangedTo(const GURL& url) override;

  void OnBlockedChanged(bool blocked);

 private:
  base::scoped_nsobject<STWebpageController> platform_controller_;
  base::scoped_nsobject<BlockedObserver> blocked_observer_;
  BlockedChangedCallback blocked_changed_callback_;
};

}  // namespace screentime

#endif  // CHROME_BROWSER_UI_COCOA_SCREENTIME_WEBPAGE_CONTROLLER_IMPL_H_
