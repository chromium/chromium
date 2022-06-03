// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_L10N_UTIL_H_
#define CHROME_BROWSER_UI_COCOA_L10N_UTIL_H_

#import <Cocoa/Cocoa.h>

namespace cocoa_l10n_util {

// Generates a tooltip string for a given URL and title.
NSString* TooltipForURLAndTitle(NSString* url, NSString* title);

// Set or clear the keys in NSUserDefaults which control UI direction based on
// whether direction is forced by a Chrome flag. This should be early in
// Chrome's launch, before any views or windows have been created, because it's
// cached by AppKit.
void ApplyForcedRTL();

}  // namespace cocoa_l10n_util

#endif  // CHROME_BROWSER_UI_COCOA_L10N_UTIL_H_
