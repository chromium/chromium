// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/window_size_autosaver.h"

#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

// If the window width stored in the prefs is smaller than this, the size is
// not restored but instead cleared from the profile -- to protect users from
// accidentally making their windows very small and then not finding them again.
const int kMinWindowWidth = 101;

// Minimum restored window height, see |kMinWindowWidth|.
const int kMinWindowHeight = 17;

@interface WindowSizeAutosaver (Private)
- (void)save:(NSNotification*)notification;
- (void)restore;
@end

@implementation WindowSizeAutosaver

- (instancetype)initWithWindow:(NSWindow*)window
                   prefService:(PrefService*)prefs
                          path:(const char*)path {
  if ((self = [super init])) {
    _window = window;
    _prefService = prefs;
    _path = path;

    [self restore];
    [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(save:)
             name:NSWindowDidMoveNotification
           object:_window];
    [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(save:)
             name:NSWindowDidResizeNotification
           object:_window];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)save:(NSNotification*)notification {
  DictionaryPrefUpdate update(_prefService, _path);
  base::Value* windowPrefs = update.Get();
  NSRect frame = [_window frame];
  if ([_window styleMask] & NSResizableWindowMask) {
    // Save the origin of the window.
    windowPrefs->SetIntKey("left", NSMinX(frame));
    windowPrefs->SetIntKey("right", NSMaxX(frame));
    // windows's and linux's profiles have top < bottom due to having their
    // screen origin in the upper left, while cocoa's is in the lower left. To
    // keep the top < bottom invariant, store top in bottom and vice versa.
    windowPrefs->SetIntKey("top", NSMinY(frame));
    windowPrefs->SetIntKey("bottom", NSMaxY(frame));
  } else {
    // Save the origin of the window.
    windowPrefs->SetIntKey("x", frame.origin.x);
    windowPrefs->SetIntKey("y", frame.origin.y);
  }
}

- (void)restore {
  // Get the positioning information.
  const base::Value* windowPrefs = _prefService->GetDictionary(_path);
  if ([_window styleMask] & NSResizableWindowMask) {
    absl::optional<int> x1 = windowPrefs->FindIntKey("left");
    absl::optional<int> x2 = windowPrefs->FindIntKey("right");
    absl::optional<int> y1 = windowPrefs->FindIntKey("top");
    absl::optional<int> y2 = windowPrefs->FindIntKey("bottom");
    if (!x1.has_value() || !x2.has_value() || !y1.has_value() ||
        !y2.has_value()) {
      return;
    }
    if (x2.value() - x1.value() < kMinWindowWidth ||
        y2.value() - y1.value() < kMinWindowHeight) {
      // Windows should never be very small.
      DictionaryPrefUpdate update(_prefService, _path);
      base::Value* mutableWindowPrefs = update.Get();
      mutableWindowPrefs->RemoveKey("left");
      mutableWindowPrefs->RemoveKey("right");
      mutableWindowPrefs->RemoveKey("top");
      mutableWindowPrefs->RemoveKey("bottom");
    } else {
      [_window
          setFrame:NSMakeRect(x1.value(), y1.value(), x2.value() - x1.value(),
                              y2.value() - y1.value())
           display:YES];

      // Make sure the window is on-screen.
      [_window cascadeTopLeftFromPoint:NSZeroPoint];
    }
  } else {
    absl::optional<int> x = windowPrefs->FindIntKey("x");
    absl::optional<int> y = windowPrefs->FindIntKey("y");
    if (!x.has_value() || !y.has_value())
      return;  // Nothing stored.
    // Turn the origin (lower-left) into an upper-left window point.
    NSPoint upperLeft =
        NSMakePoint(x.value(), y.value() + NSHeight([_window frame]));
    [_window cascadeTopLeftFromPoint:upperLeft];
  }
}

@end

