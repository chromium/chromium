// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_ios.h"

#import <Foundation/Foundation.h>
#include <stddef.h>
#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "components/policy/core/common/mac_util.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"

// This policy loader loads a managed app configuration from the NSUserDefaults.
// For example code from Apple see:
// https://developer.apple.com/library/ios/samplecode/sc2279/Introduction/Intro.html
// For an introduction to the API see session 301 from WWDC 2013,
// "Extending Your Apps for Enterprise and Education Use":
// https://developer.apple.com/videos/wwdc/2013/?id=301

namespace {

// Key in the NSUserDefaults that contains the managed app configuration.
NSString* const kConfigurationKey = @"com.apple.configuration.managed";

// Key in the managed app configuration that contains the Chrome policy.
NSString* const kChromePolicyKey = @"ChromePolicy";

// Key in the managed app configuration that contains the encoded Chrome policy.
// This is a serialized Property List, encoded in base 64.
NSString* const kEncodedChromePolicyKey = @"EncodedChromePolicy";

}  // namespace

// Helper that observes notifications for NSUserDefaults and triggers an update
// at the loader on the right thread.
@interface PolicyNotificationObserver : NSObject {
  base::Closure callback_;
  scoped_refptr<base::SequencedTaskRunner> taskRunner_;
}

// Designated initializer. |callback| will be posted to |taskRunner| whenever
// the NSUserDefaults change.
- (id)initWithCallback:(const base::Closure&)callback
            taskRunner:(scoped_refptr<base::SequencedTaskRunner>)taskRunner;

// Invoked when the NSUserDefaults change.
- (void)userDefaultsChanged:(NSNotification*)notification;

- (void)dealloc;

@end

@implementation PolicyNotificationObserver

- (id)initWithCallback:(const base::Closure&)callback
            taskRunner:(scoped_refptr<base::SequencedTaskRunner>)taskRunner {
  if ((self = [super init])) {
    callback_ = callback;
    taskRunner_ = taskRunner;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(userDefaultsChanged:)
               name:NSUserDefaultsDidChangeNotification
             object:nil];
  }
  return self;
}

- (void)userDefaultsChanged:(NSNotification*)notification {
  // This may be invoked on any thread. Post the |callback_| to the loader's
  // |taskRunner_| to make sure it Reloads() on the right thread.
  taskRunner_->PostTask(FROM_HERE, callback_);
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

@end

namespace policy {

PolicyLoaderIOS::PolicyLoaderIOS(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : AsyncPolicyLoader(task_runner),
      weak_factory_(this) {}

PolicyLoaderIOS::~PolicyLoaderIOS() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
}

void PolicyLoaderIOS::InitOnBackgroundThread() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  base::Closure callback = base::Bind(&PolicyLoaderIOS::UserDefaultsChanged,
                                      weak_factory_.GetWeakPtr());
  notification_observer_.reset(
      [[PolicyNotificationObserver alloc] initWithCallback:callback
                                                taskRunner:task_runner()]);
}

std::unique_ptr<PolicyBundle> PolicyLoaderIOS::Load() {
  std::unique_ptr<PolicyBundle> bundle(new PolicyBundle());
  NSDictionary* configuration = [[NSUserDefaults standardUserDefaults]
      dictionaryForKey:kConfigurationKey];
  id chromePolicy = configuration[kChromePolicyKey];
  id encodedChromePolicy = configuration[kEncodedChromePolicyKey];

  if (chromePolicy && [chromePolicy isKindOfClass:[NSDictionary class]]) {
    LoadNSDictionaryToPolicyBundle(chromePolicy, bundle.get());

    if (encodedChromePolicy)
      NSLog(@"Ignoring EncodedChromePolicy because ChromePolicy is present.");
  } else if (encodedChromePolicy &&
             [encodedChromePolicy isKindOfClass:[NSString class]]) {
    base::scoped_nsobject<NSData> data(
        [[NSData alloc] initWithBase64EncodedString:encodedChromePolicy
                                            options:0]);
    if (!data) {
      NSLog(@"Invalid Base64 encoding of EncodedChromePolicy");
    } else {
      NSError* error = nil;
      NSDictionary* properties = [NSPropertyListSerialization
          propertyListWithData:data.get()
                       options:NSPropertyListImmutable
                        format:NULL
                         error:&error];
      if (error) {
        NSLog(@"Invalid property list in EncodedChromePolicy: %@", error);
      } else if (!properties) {
        NSLog(@"Failed to deserialize a valid Property List");
      } else if (![properties isKindOfClass:[NSDictionary class]]) {
        NSLog(@"Invalid property list in EncodedChromePolicy: expected an "
               "NSDictionary but found %@", [properties class]);
      } else {
        LoadNSDictionaryToPolicyBundle(properties, bundle.get());
      }
    }
  }

  const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, std::string());
  size_t count = bundle->Get(chrome_ns).size();
  UMA_HISTOGRAM_COUNTS_100("Enterprise.IOSPolicies", count);

  return bundle;
}

base::Time PolicyLoaderIOS::LastModificationTime() {
  return last_notification_time_;
}

void PolicyLoaderIOS::UserDefaultsChanged() {
  // The base class coalesces multiple Reload() calls into a single Load() if
  // the LastModificationTime() has a small delta between Reload() calls.
  // This coalesces the multiple notifications sent during startup into a single
  // Load() call.
  last_notification_time_ = base::Time::Now();
  Reload(false);
}

// static
void PolicyLoaderIOS::LoadNSDictionaryToPolicyBundle(NSDictionary* dictionary,
                                                     PolicyBundle* bundle) {
  // NSDictionary is toll-free bridged to CFDictionaryRef, which is a
  // CFPropertyListRef.
  std::unique_ptr<base::Value> value =
      PropertyToValue(static_cast<CFPropertyListRef>(dictionary));
  base::DictionaryValue* dict = NULL;
  if (value && value->GetAsDictionary(&dict)) {
    PolicyMap& map = bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));
    map.LoadFrom(dict, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_PLATFORM);
  }
}

}  // namespace policy
