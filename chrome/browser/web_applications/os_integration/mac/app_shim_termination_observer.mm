// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/os_integration/mac/app_shim_termination_observer.h"

#import <Cocoa/Cocoa.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/public/browser/browser_thread.h"

@implementation AppShimTerminationObserver {
  NSRunningApplication* __strong _app;
  base::OnceClosure _callback;
}

+ (NSMutableSet<AppShimTerminationObserver*>*)allObservers {
  static NSMutableSet<AppShimTerminationObserver*>* set = [NSMutableSet set];
  return set;
}

+ (void)startObservingForRunningApplication:(NSRunningApplication*)app
                               withCallback:(base::OnceClosure)callback {
  AppShimTerminationObserver* observer = [[AppShimTerminationObserver alloc]
      initWithRunningApplication:app
                        callback:std::move(callback)];

  if (observer) {
    [[AppShimTerminationObserver allObservers] addObject:observer];
  }
}

- (instancetype)initWithRunningApplication:(NSRunningApplication*)app
                                  callback:(base::OnceClosure)callback {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (self = [super init]) {
    _callback = std::move(callback);
    _app = app;
    // Note that |observeValueForKeyPath| will be called with the initial value
    // within the |addObserver| call.
    [_app addObserver:self
           forKeyPath:@"isTerminated"
              options:NSKeyValueObservingOptionNew |
                      NSKeyValueObservingOptionInitial
              context:nullptr];
  }
  return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  NSNumber* newNumberValue = change[NSKeyValueChangeNewKey];
  BOOL newValue = newNumberValue.boolValue;
  if (newValue) {
    // Note that a block is posted, which will hold a retain on `self`.
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(^{
                                                   [self onTerminated];
                                                 }));
  }
}

- (void)onTerminated {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If |onTerminated| is called repeatedly (which in theory it should not),
  // then ensure that we only call removeObserver and release once by doing an
  // early-out if |callback_| has already been made.
  if (!_callback) {
    return;
  }
  std::move(_callback).Run();
  DCHECK(!_callback);

  [_app removeObserver:self forKeyPath:@"isTerminated" context:nullptr];

  [[AppShimTerminationObserver allObservers]
      performSelector:@selector(removeObject:)
           withObject:self
           afterDelay:0];
}
@end
