// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/handoff/handoff_manager.h"

#include "base/logging.h"
#include "base/mac/objc_release_properties.h"
#include "base/mac/scoped_nsobject.h"
#include "net/base/mac/url_conversions.h"

#if defined(OS_IOS)
#include "components/handoff/pref_names_ios.h"
#include "components/pref_registry/pref_registry_syncable.h"  // nogncheck
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#endif

@interface HandoffManager ()

// The active user activity.
@property(nonatomic, retain) NSUserActivity* userActivity;

// Whether the URL of the current tab should be exposed for Handoff.
- (BOOL)shouldUseActiveURL;

// Updates the active NSUserActivity.
- (void)updateUserActivity;

@end

@implementation HandoffManager {
  GURL _activeURL;
  NSUserActivity* _userActivity;
  handoff::Origin _origin;
}

@synthesize userActivity = _userActivity;

#if defined(OS_IOS)
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterBooleanPref(
      prefs::kIosHandoffToOtherDevices, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}
#endif

- (instancetype)init {
  self = [super init];
  if (self) {
#if defined(OS_MACOSX) && !defined(OS_IOS)
    _origin = handoff::ORIGIN_MAC;
#elif defined(OS_IOS)
    _origin = handoff::ORIGIN_IOS;
#else
    NOTREACHED();
#endif
  }
  return self;
}

- (void)dealloc {
  base::mac::ReleaseProperties(self);
  [super dealloc];
}

- (void)updateActiveURL:(const GURL&)url {
  _activeURL = url;
  [self updateUserActivity];
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
  const GURL userActivityURL(net::GURLWithNSURL(self.userActivity.webpageURL));
  if (userActivityURL == _activeURL)
    return;

  // Invalidate the old user activity and make a new one.
  [self.userActivity invalidate];

  base::scoped_nsobject<NSUserActivity> userActivity([[NSUserActivity alloc]
      initWithActivityType:NSUserActivityTypeBrowsingWeb]);
  self.userActivity = userActivity;
  self.userActivity.webpageURL = net::NSURLWithGURL(_activeURL);
  NSString* origin = handoff::StringFromOrigin(_origin);
  DCHECK(origin);
  self.userActivity.userInfo = @{ handoff::kOriginKey : origin };
  [self.userActivity becomeCurrent];
}

@end

#if defined(OS_IOS)
@implementation HandoffManager (TestingOnly)

- (NSURL*)userActivityWebpageURL {
  return self.userActivity.webpageURL;
}

@end
#endif
