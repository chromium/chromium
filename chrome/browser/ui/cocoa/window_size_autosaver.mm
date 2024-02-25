// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/window_size_autosaver.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/raw_ptr.h"
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

@implementation WindowSizeAutosaver {
  NSWindow* __weak _window;
  raw_ptr<PrefService> _prefService;  // weak
  const char* _path;
}

- (instancetype)initWithWindow:(NSWindow*)window
                   prefService:(PrefService*)prefs
                          path:(const char*)path {
  if ((self = [super init])) {
    _window = window;
    _prefService = prefs;
    _path = path;

    [self restore];
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(save:)
                                               name:NSWindowDidMoveNotification
                                             object:_window];
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(save:)
               name:NSWindowDidResizeNotification
             object:_window];
  }
  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)save:(NSNotification*)notification {
  ScopedDictPrefUpdate update(_prefService, _path);
  base::Value::Dict& windowPrefs = update.Get();
  NSRect frame = [_window frame];
  if ([_window styleMask] & NSWindowStyleMaskResizable) {
    // Save the origin of the window.
    windowPrefs.Set("left", static_cast<int>(NSMinX(frame)));
    windowPrefs.Set("right", static_cast<int>(NSMaxX(frame)));
    // windows's and linux's profiles have top < bottom due to having their
    // screen origin in the upper left, while cocoa's is in the lower left. To
    // keep the top < bottom invariant, store top in bottom and vice versa.
    windowPrefs.Set("top", static_cast<int>(NSMinY(frame)));
    windowPrefs.Set("bottom", static_cast<int>(NSMaxY(frame)));
  } else {
    // Save the origin of the window.
    windowPrefs.Set("x", static_cast<int>(frame.origin.x));
    windowPrefs.Set("y", static_cast<int>(frame.origin.y));
  }
}

- (void)restore {
  // Get the positioning information.
  const base::Value::Dict& windowPrefs = _prefService->GetDict(_path);
  if ([_window styleMask] & NSWindowStyleMaskResizable) {
    std::optional<int> x1 = windowPrefs.FindInt("left");
    std::optional<int> x2 = windowPrefs.FindInt("right");
    std::optional<int> y1 = windowPrefs.FindInt("top");
    std::optional<int> y2 = windowPrefs.FindInt("bottom");
    if (!x1.has_value() || !x2.has_value() || !y1.has_value() ||
        !y2.has_value()) {
      return;
    }
    if (x2.value() - x1.value() < kMinWindowWidth ||
        y2.value() - y1.value() < kMinWindowHeight) {
      // Windows should never be very small.
      ScopedDictPrefUpdate update(_prefService, _path);
      base::Value::Dict& mutableWindowPrefs = update.Get();
      mutableWindowPrefs.Remove("left");
      mutableWindowPrefs.Remove("right");
      mutableWindowPrefs.Remove("top");
      mutableWindowPrefs.Remove("bottom");
    } else {
      [_window
          setFrame:NSMakeRect(x1.value(), y1.value(), x2.value() - x1.value(),
                              y2.value() - y1.value())
           display:YES];

      // Make sure the window is on-screen.
      [_window cascadeTopLeftFromPoint:NSZeroPoint];
    }
  } else {
    std::optional<int> x = windowPrefs.FindInt("x");
    std::optional<int> y = windowPrefs.FindInt("y");
    if (!x.has_value() || !y.has_value())
      return;  // Nothing stored.
    // Turn the origin (lower-left) into an upper-left window point.
    NSPoint upperLeft =
        NSMakePoint(x.value(), y.value() + NSHeight([_window frame]));
    [_window cascadeTopLeftFromPoint:upperLeft];
  }
}

@end
