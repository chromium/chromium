// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/handoff/handoff_manager.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "net/base/apple/url_conversions.h"

#if BUILDFLAG(IS_IOS)
#include "components/handoff/pref_names_ios.h"
#include "components/pref_registry/pref_registry_syncable.h"  // nogncheck
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

@interface HandoffManager ()

// The active user activity.
@property(nonatomic, strong) NSUserActivity* userActivity;

// Whether the URL of the current tab should be exposed for Handoff.
- (BOOL)shouldUseActiveURL;

// Updates the active NSUserActivity.
- (void)updateUserActivity;

@end

@implementation HandoffManager {
  GURL _activeURL;
}

@synthesize userActivity = _userActivity;

#if BUILDFLAG(IS_IOS)
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterBooleanPref(
      prefs::kIosHandoffToOtherDevices, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}
#endif

- (instancetype)init {
  return [super init];
}

- (void)updateActiveURL:(const GURL&)url {
  _activeURL = url;
  [self updateUserActivity];
}

- (void)updateActiveTitle:(const std::u16string&)title {
  // Assume the activity has already been created since the page navigation
  // will complete before the page title loads. No need to re-create it, just
  // set the title. If the activity has not been created, ignore the update.
  self.userActivity.title = base::SysUTF16ToNSString(title);
}

- (BOOL)shouldUseActiveURL {
  return _activeURL.SchemeIsHTTPOrHTTPS();
}

- (void)updateUserActivity {
  // Clear the user activity.
  if (![self shouldUseActiveURL]) {
    [self.userActivity invalidate];
    self.userActivity = nil;
    return;
  }

  // No change to the user activity.
  const GURL userActivityURL = net::GURLWithNSURL(self.userActivity.webpageURL);
  if (userActivityURL == _activeURL) {
    return;
  }

  // Invalidate the old user activity and make a new one.
  [self.userActivity invalidate];

  self.userActivity = [[NSUserActivity alloc]
      initWithActivityType:NSUserActivityTypeBrowsingWeb];
  self.userActivity.webpageURL = net::NSURLWithGURL(_activeURL);
  [self.userActivity becomeCurrent];
}

@end

#if BUILDFLAG(IS_IOS)
@implementation HandoffManager (TestingOnly)

- (NSURL*)userActivityWebpageURL {
  return self.userActivity.webpageURL;
}

- (NSString*)userActivityTitle {
  return self.userActivity.title;
}

@end
#endif
