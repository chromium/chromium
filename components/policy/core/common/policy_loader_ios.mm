// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_ios.h"

#import <Foundation/Foundation.h>
#include <stddef.h>
#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/mac_util.h"
#include "components/policy/core/common/policy_bundle.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This policy loader loads a managed app configuration from the NSUserDefaults.
// For example code from Apple see:
// https://developer.apple.com/library/ios/samplecode/sc2279/Introduction/Intro.html
// For an introduction to the API see session 301 from WWDC 2013,
// "Extending Your Apps for Enterprise and Education Use":
// https://developer.apple.com/videos/wwdc/2013/?id=301

// Helper that observes notifications for NSUserDefaults and triggers an update
// at the loader on the right thread.
@interface PolicyNotificationObserver : NSObject {
  base::RepeatingClosure _callback;
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
}

// Designated initializer. |callback| will be posted to |taskRunner| whenever
// the NSUserDefaults change.
- (id)initWithCallback:(const base::RepeatingClosure&)callback
            taskRunner:(scoped_refptr<base::SequencedTaskRunner>)taskRunner;

// Invoked when the NSUserDefaults change.
- (void)userDefaultsChanged:(NSNotification*)notification;

- (void)dealloc;

@end

@implementation PolicyNotificationObserver

- (id)initWithCallback:(const base::RepeatingClosure&)callback
            taskRunner:(scoped_refptr<base::SequencedTaskRunner>)taskRunner {
  if ((self = [super init])) {
    _callback = callback;
    _taskRunner = taskRunner;
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
  _taskRunner->PostTask(FROM_HERE, _callback);
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

@end

namespace policy {

PolicyLoaderIOS::PolicyLoaderIOS(
    SchemaRegistry* registry,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : AsyncPolicyLoader(task_runner, /*periodic_updates=*/true),
      weak_factory_(this) {
  PolicyNamespace ns(POLICY_DOMAIN_CHROME, std::string());
  policy_schema_ = registry->schema_map()->GetSchema(ns);
}

PolicyLoaderIOS::~PolicyLoaderIOS() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
}

void PolicyLoaderIOS::InitOnBackgroundThread() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  base::RepeatingClosure callback = base::BindRepeating(
      &PolicyLoaderIOS::UserDefaultsChanged, weak_factory_.GetWeakPtr());
  notification_observer_ =
      [[PolicyNotificationObserver alloc] initWithCallback:callback
                                                taskRunner:task_runner()];
}

PolicyBundle PolicyLoaderIOS::Load() {
  PolicyBundle bundle;
  NSDictionary* configuration = [[NSUserDefaults standardUserDefaults]
      dictionaryForKey:kPolicyLoaderIOSConfigurationKey];
  LoadNSDictionaryToPolicyBundle(configuration, &bundle);

  const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, std::string());
  size_t count = bundle.Get(chrome_ns).size();
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

void PolicyLoaderIOS::LoadNSDictionaryToPolicyBundle(NSDictionary* dictionary,
                                                     PolicyBundle* bundle) {
  // NSDictionary is toll-free bridged to CFDictionaryRef, which is a
  // CFPropertyListRef.
  std::unique_ptr<base::Value> value =
      PropertyToValue((__bridge CFPropertyListRef)(dictionary));
  if (!value || !value->is_dict())
    return;

  PolicyMap& map = bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));
  for (const auto it : value->GetDict()) {
    map.Set(it.first, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
            POLICY_SOURCE_PLATFORM,
            ConvertPolicyDataIfNecessary(it.first, it.second), nullptr);
  }
}

base::Value PolicyLoaderIOS::ConvertPolicyDataIfNecessary(
    const std::string& key,
    const base::Value& value) {
  const Schema schema = policy_schema_->GetKnownProperty(key);

  if (!schema.valid()) {
    return value.Clone();
  }

  // Handle the case of a JSON-encoded string for a dict policy.
  if ((schema.type() == base::Value::Type::DICTIONARY ||
       schema.type() == base::Value::Type::LIST) &&
      value.is_string()) {
    absl::optional<base::Value> decoded_value = base::JSONReader::Read(
        value.GetString(), base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    if (decoded_value.has_value()) {
      return std::move(decoded_value.value());
    }
  }

  // Otherwise return an unchanged value.
  return value.Clone();
}

}  // namespace policy
