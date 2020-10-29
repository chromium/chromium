// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include <xpc/xpc.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/notifications/service_delegate.h"

// The main method of the notification alert xpc service.
// It is initiaized by Chrome on demand whenever a notification of type alert
// needs to be displayed.
// The connection will then remain open in order to receive notification
// clicks and so until all notifications of type alert are closed.
int main(int argc, const char* argv[]) {
  @autoreleasepool {
    base::scoped_nsobject<ServiceDelegate> delegate(
        [[ServiceDelegate alloc] init]);

    NSXPCListener* listener = [NSXPCListener serviceListener];
    listener.delegate = delegate.get();

    [listener resume];

    return 0;
  }
}
