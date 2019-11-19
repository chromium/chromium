// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/handoff_active_url_observer_bridge.h"

#include "chrome/browser/ui/cocoa/handoff_active_url_observer.h"

HandoffActiveURLObserverBridge::HandoffActiveURLObserverBridge(
    NSObject<HandoffActiveURLObserverBridgeDelegate>* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  observer_ = std::make_unique<HandoffActiveURLObserver>(this);
}

HandoffActiveURLObserverBridge::~HandoffActiveURLObserverBridge() {}

void HandoffActiveURLObserverBridge::HandoffActiveURLChanged(
    content::WebContents* web_contents) {
  [delegate_ handoffActiveURLChanged:web_contents];
}
