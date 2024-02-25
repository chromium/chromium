// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/policy/core/common/policy_loader_ios.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <stddef.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/json/json_reader.h"
#import "base/location.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/policy/core/common/mac_util.h"
#import "components/policy/core/common/policy_bundle.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/policy_namespace.h"
#import "components/policy/core/common/schema.h"
#import "components/policy/core/common/schema_registry.h"
#import "components/policy/policy_constants.h"

namespace {

// Policy reload interval when the browser has platform policy key.
constexpr base::TimeDelta kManagedByPlatformReloadInterval = base::Seconds(30);

// Returns YES if the browser has platform policy key by looking at the
// presence of the App Config key. Even if the value of the key is an empty
// dictionary, the browser will be considered as managed.
BOOL HasPlatformPolicyKey() {
  return [[NSUserDefaults standardUserDefaults]
             dictionaryForKey:kPolicyLoaderIOSConfigurationKey] != nil;
}

}  // namespace

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

void PolicyLoaderIOS::InitOnBackgroundThread() {}

PolicyBundle PolicyLoaderIOS::Load() {
  PolicyBundle bundle;
  NSDictionary* configuration = [[NSUserDefaults standardUserDefaults]
      dictionaryForKey:kPolicyLoaderIOSConfigurationKey];
  LoadNSDictionaryToPolicyBundle(configuration, &bundle);

  if (HasPlatformPolicyKey()) {
    // Set a shorter reload interval when the browser is managed by the
    // platform. This is to take the dynamic policy updates quickly.
    set_reload_interval(kManagedByPlatformReloadInterval);
  }

  return bundle;
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
  if ((schema.type() == base::Value::Type::DICT ||
       schema.type() == base::Value::Type::LIST) &&
      value.is_string()) {
    std::optional<base::Value> decoded_value = base::JSONReader::Read(
        value.GetString(), base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    if (decoded_value.has_value()) {
      return std::move(decoded_value.value());
    }
  }

  // Otherwise return an unchanged value.
  return value.Clone();
}

}  // namespace policy
