// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_APPLE_EVENT_UTIL_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_APPLE_EVENT_UTIL_H_

#import <Foundation/Foundation.h>

namespace base {
class Value;
}

class Profile;

namespace AppleScript {

// Translates the given `value` to as meaningful of an Apple Event descriptor as
// can be expected.
NSAppleEventDescriptor* ValueToAppleEventDescriptor(const base::Value& value);

// Returns true if Javascript in Apple Events is enabled for `profile`.
bool IsJavaScriptEnabledForProfile(Profile* profile);

// Returns the currently executing script command. If an override is active (set
// via ScopedCurrentScriptCommand) then that overriding command is returned,
// otherwise returns +[NSScriptCommand currentCommand].
NSScriptCommand* GetCurrentScriptCommand();

// Overrides the GetCurrentScriptCommand() NSScriptCommand for the lifetime of
// this scope. Used when resuming a suspended command asynchronously outside
// AppleEvent dispatch, where +[NSScriptCommand currentCommand] returns nil, and
// for testing when there isn't a real NSScriptCommand but any error handling
// needs to be intercepted for checking.
class ScopedCurrentScriptCommand {
 public:
  explicit ScopedCurrentScriptCommand(NSScriptCommand* command);
  ScopedCurrentScriptCommand(const ScopedCurrentScriptCommand&) = delete;
  ScopedCurrentScriptCommand& operator=(const ScopedCurrentScriptCommand&) =
      delete;
  ~ScopedCurrentScriptCommand();
};

}  // namespace AppleScript

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_APPLE_EVENT_UTIL_H_
