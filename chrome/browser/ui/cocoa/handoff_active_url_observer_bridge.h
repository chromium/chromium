// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_HANDOFF_ACTIVE_URL_OBSERVER_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_HANDOFF_ACTIVE_URL_OBSERVER_BRIDGE_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#include "chrome/browser/ui/cocoa/handoff_active_url_observer_delegate.h"

namespace content {
class WebContents;
}

class HandoffActiveURLObserver;

// A protocol that allows ObjC objects to receive delegate callbacks from
// HandoffActiveURLObserver.
@protocol HandoffActiveURLObserverBridgeDelegate
- (void)handoffActiveURLChanged:(content::WebContents*)webContents;
@end

// This class allows an ObjC object to receive the delegate callbacks from an
// HandoffActiveURLObserver.
class HandoffActiveURLObserverBridge : public HandoffActiveURLObserverDelegate {
 public:
  explicit HandoffActiveURLObserverBridge(
      NSObject<HandoffActiveURLObserverBridgeDelegate>* delegate);

  HandoffActiveURLObserverBridge(const HandoffActiveURLObserverBridge&) =
      delete;
  HandoffActiveURLObserverBridge& operator=(
      const HandoffActiveURLObserverBridge&) = delete;

  ~HandoffActiveURLObserverBridge() override;

 private:
  void HandoffActiveURLChanged(content::WebContents* web_contents) override;

  // Instances of this class should be owned by their |delegate_|.
  NSObject<HandoffActiveURLObserverBridgeDelegate>* delegate_;

  // The C++ object that this class acts as a bridge for.
  std::unique_ptr<HandoffActiveURLObserver> observer_;
};

#endif  // CHROME_BROWSER_UI_COCOA_HANDOFF_ACTIVE_URL_OBSERVER_BRIDGE_H_
