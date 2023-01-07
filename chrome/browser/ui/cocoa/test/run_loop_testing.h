// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TEST_RUN_LOOP_TESTING_H_
#define CHROME_BROWSER_UI_COCOA_TEST_RUN_LOOP_TESTING_H_

namespace chrome {
namespace testing {

// A common pattern in Chromium is to get a selector to execute on the next
// iteration of the outermost run loop, done like so:
//
//    [someObj performSelector:@selector(someSel:) withObject:nil afterDelay:0];
//
// This is used when performing the work will negatively affect something
// currently on the stack. Unfortunately this also affects the testability of
// objects that do this. A call to this function will pump work like this from
// the event queue and run it until all such work, as of the time of calling
// this, has been processed.
//
// Note that this is not a NSApplication-based loop, and so things like NSEvents
// are *not* pumped from the event queue.
void NSRunLoopRunAllPending();

}  // namespace testing
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COCOA_TEST_RUN_LOOP_TESTING_H_
